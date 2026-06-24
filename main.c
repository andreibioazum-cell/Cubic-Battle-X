#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

// --- МАТЕМАТИКА ---
void mat4_identity(float* m) { for(int i=0; i<16; i++) m[i]=0; m[0]=m[5]=m[10]=m[15]=1.0f; }

void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    for(int i=0; i<16; i++) m[i]=0;
    m[0] = f / aspect; m[5] = f; m[10] = (far+near)/(near-far); m[11] = -1.0f; m[14] = (2.0f*far*near)/(near-far);
}

void mat4_translate(float* m, float x, float y, float z) { mat4_identity(m); m[12]=x; m[13]=y; m[14]=z; }
void mat4_scale(float* m, float x, float y, float z) { mat4_identity(m); m[0]=x; m[5]=y; m[10]=z; }

void mat4_mul(float* res, float* a, float* b) {
    float t[16];
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) {
        t[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
    }
    for(int i=0; i<16; i++) res[i] = t[i];
}

// --- ДАННЫЕ ИГРЫ ---
struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLint mvp_loc; GLint col_loc;
    
    float p_x, p_z;       // Позиция игрока
    float joy_x, joy_y;   // Вектор джойстика
    int touching;         // Флаг касания
};

// --- ГРАФИКА ---
static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    EGLConfig config; EGLint num;
    eglChooseConfig(eng->display, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &config, 1, &num);
    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    const char* vs = "uniform mat4 m; attribute vec4 p; void main(){gl_Position=m*p;}";
    const char* fs = "precision mediump float; uniform vec4 c; void main(){gl_FragColor=c;}";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->program = glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh); glLinkProgram(eng->program);
    eng->mvp_loc = glGetUniformLocation(eng->program, "m");
    eng->col_loc = glGetUniformLocation(eng->program, "c");
    glEnable(GL_DEPTH_TEST);
}

void draw_cube(struct engine* eng, float* proj_view, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
    float model[16], scale[16], mvp[16];
    mat4_translate(model, x, y, z);
    mat4_scale(scale, sx, sy, sz);
    mat4_mul(model, model, scale);
    mat4_mul(mvp, proj_view, model);

    float v[] = { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 };
    unsigned short ind[] = { 0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4 };

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp);
    glUniform4f(eng->col_loc, r, g, b, 1.0f);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, v);
    glEnableVertexAttribArray(0);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, ind);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;
    // ОБНОВЛЕНИЕ: Двигаем игрока
    if (eng->touching) {
        eng->p_x += eng->joy_x * 0.1f;
        eng->p_z += eng->joy_y * 0.1f;
    }

    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    glViewport(0, 0, w, h);
    // Ровный серый фон (100, 100, 100)
    glClearColor(0.39f, 0.39f, 0.39f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    float proj[16], view[16], pv[16];
    mat4_perspective(proj, 0.8f, (float)w/h, 0.1f, 100.0f);
    // Камера смотрит сверху под углом
    mat4_translate(view, -eng->p_x, -5.0f, -eng->p_z - 7.0f);
    float rotX[16]; mat4_identity(rotX); rotX[5]=0.7f; rotX[6]=0.7f; rotX[9]=-0.7f; rotX[10]=0.7f; // Наклон 45 град
    mat4_mul(pv, rotX, view);
    mat4_mul(pv, proj, pv);

    // РИСУЕМ ПОЛ (Зеленовато-серый, чтобы не сливался)
    draw_cube(eng, pv, 0, -0.5f, 0, 20.0f, 0.1f, 20.0f, 0.2f, 0.25f, 0.2f);
    
    // РИСУЕМ ПЕРСОНАЖА (Красный кубик)
    draw_cube(eng, pv, eng->p_x, 0.5f, eng->p_z, 1.0f, 1.0f, 1.0f, 0.8f, 0.1f, 0.1f);

    eglSwapBuffers(eng->display, eng->surface);
}

// --- ВВОД (INPUT) ---
static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        int w = ANativeWindow_getWidth(app->window);

        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_MOVE) {
            // Если жмем в левой части экрана - это джойстик
            if (x < w / 2) {
                eng->touching = 1;
                // Центр джойстика примерно в (200, 800)
                eng->joy_x = (x - 200.0f) / 100.0f;
                eng->joy_y = (y - (ANativeWindow_getHeight(app->window) - 300.0f)) / 100.0f;
                // Ограничиваем скорость
                float len = sqrtf(eng->joy_x*eng->joy_x + eng->joy_y*eng->joy_y);
                if (len > 1.0f) { eng->joy_x /= len; eng->joy_y /= len; }
            }
        } else {
            eng->touching = 0;
        }
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) init_gl(eng);
    else if (cmd == APP_CMD_TERM_WINDOW) eng->display = NULL;
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    eng.app = state;
    while (1) {
        int events; struct android_poll_source* source;
        while (ALooper_pollOnce(eng.display ? 0 : -1, 0, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        if (eng.display) draw(&eng);
    }
}

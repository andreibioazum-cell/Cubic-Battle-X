#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

// --- Математика ---
typedef struct { float m[16]; } mat4;
void mat4_identity(mat4* m) { for(int i=0; i<16; i++) m->m[i]=0; m->m[0]=m->m[5]=m->m[10]=m->m[15]=1.0f; }

void mat4_perspective(mat4* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    mat4_identity(m);
    m->m[0] = f / aspect; m->m[5] = f;
    m->m[10] = (far + near) / (near - far); m->m[11] = -1.0f;
    m->m[14] = (2.0f * far * near) / (near - far); m->m[15] = 0;
}

void mat4_mul(mat4* res, mat4* a, mat4* b) {
    mat4 t;
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) {
        t.m[i*4+j] = a->m[i*4+0]*b->m[0*4+j] + a->m[i*4+1]*b->m[1*4+j] + a->m[i*4+2]*b->m[2*4+j] + a->m[i*4+3]*b->m[3*4+j];
    }
    *res = t;
}

// --- Структуры ---
struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLint mvp_loc; GLint col_loc;
    
    float p_x, p_z;       // Позиция игрока
    float joy_start_x, joy_start_y; // Точка первого нажатия
    float cur_joy_x, cur_joy_y;     // Текущее смещение
    int is_touching;
};

// --- Графика ---
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

void draw_obj(struct engine* eng, mat4* pv, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
    mat4 model, scale, mvp;
    mat4_identity(&model); model.m[12]=x; model.m[13]=y; model.m[14]=z;
    mat4_identity(&scale); scale.m[0]=sx; scale.m[5]=sy; scale.m[10]=sz;
    mat4_mul(&model, &model, &scale);
    mat4_mul(&mvp, pv, &model);

    float v[] = { -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5 };
    unsigned short ind[] = { 0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4 };

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp.m);
    glUniform4f(eng->col_loc, r, g, b, 1.0f);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, v);
    glEnableVertexAttribArray(0);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, ind);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;
    
    // Движение
    if (eng->is_touching) {
        float dx = eng->cur_joy_x - eng->joy_start_x;
        float dz = eng->cur_joy_y - eng->joy_start_y;
        float len = sqrtf(dx*dx + dz*dz);
        if (len > 10.0f) { // Мертвая зона
            eng->p_x += (dx / len) * 0.08f;
            eng->p_z += (dz / len) * 0.08f;
        }
    }

    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    glViewport(0, 0, w, h);
    glClearColor(0.4f, 0.4f, 0.4f, 1.0f); // Твой серый фон
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    mat4 proj, view, pv, rot;
    mat4_perspective(&proj, 0.7f, (float)w/h, 0.1f, 100.0f);
    
    // Камера: смотрим сверху вниз (X-наклон)
    mat4_identity(&view);
    view.m[12] = -eng->p_x;
    view.m[13] = -8.0f; // Высота камеры
    view.m[14] = -eng->p_z - 12.0f; // Дистанция
    
    mat4_identity(&rot);
    rot.m[5] = 0.86f; rot.m[6] = 0.5f; rot.m[9] = -0.5f; rot.m[10] = 0.86f; // Наклон 30 градусов
    
    mat4_mul(&pv, &rot, &view);
    mat4_mul(&pv, &proj, &pv);

    // ПОЛ (Зеленовато-серый)
    draw_obj(eng, &pv, 0, 0, 0, 40.0f, 0.1f, 40.0f, 0.3f, 0.35f, 0.3f);
    // ПЕРСОНАЖ (Красный)
    draw_obj(eng, &pv, eng->p_x, 0.5f, eng->p_z, 1.2f, 1.2f, 1.2f, 0.9f, 0.1f, 0.1f);

    eglSwapBuffers(eng->display, eng->surface);
}

// --- Управление ---
static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);

        if (action == AMOTION_EVENT_ACTION_DOWN) {
            if (x < ANativeWindow_getWidth(app->window) / 2) {
                eng->is_touching = 1;
                eng->joy_start_x = x; eng->joy_start_y = y;
                eng->cur_joy_x = x; eng->cur_joy_y = y;
            }
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            if (eng->is_touching) {
                eng->cur_joy_x = x; eng->cur_joy_y = y;
            }
        } else if (action == AMOTION_EVENT_ACTION_UP) {
            eng->is_touching = 0;
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
    state->userData = &eng; state->onAppCmd = handle_cmd; state->onInputEvent = handle_input;
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

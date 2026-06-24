#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

// --- ПРАВИЛЬНАЯ МАТЕМАТИКА (Column-Major) ---
typedef struct { float m[16]; } mat4;

void mat4_identity(mat4* m) {
    for(int i=0; i<16; i++) m->m[i]=0;
    m->m[0]=m->m[5]=m->m[10]=m->m[15]=1.0f;
}

void mat4_perspective(mat4* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    mat4_identity(m);
    m->m[0] = f / aspect;
    m->m[5] = f;
    m->m[10] = (far + near) / (near - far);
    m->m[11] = -1.0f;
    m->m[14] = (2.0f * far * near) / (near - far);
    m->m[15] = 0;
}

void mat4_mul(mat4* res, mat4* a, mat4* b) {
    mat4 t;
    for (int i = 0; i < 4; i++) { // столбец
        for (int j = 0; j < 4; j++) { // строка
            t.m[i*4+j] = a->m[0*4+j]*b->m[i*4+0] + a->m[1*4+j]*b->m[i*4+1] +
                         a->m[2*4+j]*b->m[i*4+2] + a->m[3*4+j]*b->m[i*4+3];
        }
    }
    *res = t;
}

// --- ДВИЖОК ---
struct engine {
    struct android_app* app;
    EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLint mvp_loc; GLint col_loc;
    GLuint vbo;

    float p_x, p_z;
    float joy_sx, joy_sy, cur_x, cur_y;
    int touching;
};

static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    
    EGLConfig config; EGLint num;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
    eglChooseConfig(eng->display, attribs, &config, 1, &num);
    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    const char* vs = "uniform mat4 u_mvp; attribute vec4 a_pos; void main(){gl_Position = u_mvp * a_pos;}";
    const char* fs = "precision mediump float; uniform vec4 u_col; void main(){gl_FragColor = u_col;}";
    
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->program = glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh);
    glLinkProgram(eng->program);
    eng->mvp_loc = glGetUniformLocation(eng->program, "u_mvp");
    eng->col_loc = glGetUniformLocation(eng->program, "u_col");

    float cube[] = {
        -0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5,
        -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5
    };
    glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube), cube, GL_STATIC_DRAW);
    
    glEnable(GL_DEPTH_TEST);
}

void draw_box(struct engine* eng, mat4* pv, float x, float y, float z, float sx, float sy, float sz, float r, float g, float b) {
    mat4 model, mvp;
    mat4_identity(&model);
    model.m[12]=x; model.m[13]=y; model.m[14]=z;
    model.m[0]=sx; model.m[5]=sy; model.m[10]=sz;
    mat4_mul(&mvp, pv, &model);

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp.m);
    glUniform4f(eng->col_loc, r, g, b, 1.0f);
    
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, 0);
    glEnableVertexAttribArray(0);
    
    unsigned short indices[] = {0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4};
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;

    if (eng->touching) {
        float dx = eng->cur_x - eng->joy_sx;
        float dy = eng->cur_y - eng->joy_sy;
        float d = sqrtf(dx*dx + dy*dy);
        if (d > 5.0f) {
            eng->p_x += (dx/d) * 0.1f;
            eng->p_z += (dy/d) * 0.1f;
        }
    }

    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    glViewport(0, 0, w, h);
    glClearColor(0.39f, 0.39f, 0.39f, 1.0f); // Твой серый (100,100,100)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    mat4 proj, view, pv;
    mat4_perspective(&proj, 0.78f, (float)w/h, 0.1f, 100.0f);
    
    mat4_identity(&view);
    // Камера наклонена и следит за игроком
    view.m[14] = -15.0f; // Дистанция
    view.m[13] = -10.0f; // Высота
    view.m[10] = 0.7f; view.m[6] = 0.7f; // Наклон камеры
    view.m[9] = -0.7f; view.m[5] = 0.7f;
    
    mat4 move; mat4_identity(&move);
    move.m[12] = -eng->p_x; move.m[14] = -eng->p_z;
    mat4_mul(&view, &view, &move);
    mat4_mul(&pv, &proj, &view);

    // ПОЛ
    draw_box(eng, &pv, 0, 0, 0, 50.0f, 0.1f, 50.0f, 0.3f, 0.3f, 0.3f);
    // ИГРОК
    draw_box(eng, &pv, eng->p_x, 1.0f, eng->p_z, 1.5f, 1.5f, 1.5f, 1.0f, 0.1f, 0.1f);

    eglSwapBuffers(eng->display, eng->surface);
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(event, 0);
        float y = AMotionEvent_getY(event, 0);
        if (action == AMOTION_EVENT_ACTION_DOWN && x < ANativeWindow_getWidth(app->window)/2) {
            eng->touching = 1; eng->joy_sx = x; eng->joy_sy = y; eng->cur_x = x; eng->cur_y = y;
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            eng->cur_x = x; eng->cur_y = y;
        } else if (action == AMOTION_EVENT_ACTION_UP) {
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

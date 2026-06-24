#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLint mvp_loc; float angle;
};

// Функция для создания матрицы проекции
void get_projection(float* m, float aspect) {
    float fov = 1.0f / tanf(45.0f * 0.5f * M_PI / 180.0f);
    float near = 0.1f, far = 100.0f;
    for(int i=0; i<16; i++) m[i]=0;
    m[0] = fov / aspect; m[5] = fov;
    m[10] = (far + near) / (near - far); m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

// Функция для создания матрицы вращения и перемещения
void get_model_view(float* m, float angle) {
    for(int i=0; i<16; i++) m[i]=0;
    float s = sinf(angle), c = cosf(angle);
    // Вращение вокруг Y и X одновременно
    m[0] = c;  m[2] = -s;
    m[4] = s*s; m[5] = c; m[6] = c*s;
    m[8] = s*c; m[9] = -s; m[10] = c*c;
    m[15] = 1.0f;
    m[14] = -3.0f; // Отодвигаем куб назад на 3 единицы
}

static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    EGLConfig config; EGLint num;
    eglChooseConfig(eng->display, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &config, 1, &num);
    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    // Шейдеры с высокой точностью
    const char* vs = 
        "uniform mat4 u_proj; uniform mat4 u_model; attribute vec4 a_pos; attribute vec4 a_col; "
        "varying vec4 v_col; void main() { gl_Position = u_proj * u_model * a_pos; v_col = a_col; }";
    const char* fs = "precision highp float; varying vec4 v_col; void main() { gl_FragColor = v_col; }";
    
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->program = glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh);
    glLinkProgram(eng->program);
    glEnable(GL_DEPTH_TEST);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.15f, 0.2f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    // Данные КУБА (Позиции + Цвета)
    float v[] = {
        -0.5,-0.5, 0.5, 1,0,0,  0.5,-0.5, 0.5, 0,1,0,  0.5, 0.5, 0.5, 0,0,1, -0.5, 0.5, 0.5, 1,1,0, // перед
        -0.5,-0.5,-0.5, 1,0,1,  0.5,-0.5,-0.5, 0,1,1,  0.5, 0.5,-0.5, 1,1,1, -0.5, 0.5,-0.5, 0.5,0.5,0.5 // зад
    };
    unsigned short indices[] = { 0,1,2, 2,3,0, 1,5,6, 6,2,1, 7,6,5, 5,4,7, 4,0,3, 3,7,4, 3,2,6, 6,7,3, 4,5,1, 1,0,4 };

    float proj[16], model[16];
    get_projection(proj, (float)w/(float)h);
    get_model_view(model, eng->angle);

    // Передаем матрицы в шейдер по отдельности!
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "u_proj"), 1, 0, proj);
    glUniformMatrix4fv(glGetUniformLocation(eng->program, "u_model"), 1, 0, model);

    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, v); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &v[3]); glEnableVertexAttribArray(1);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices);
    eglSwapBuffers(eng->display, eng->surface);
    eng->angle += 0.02f;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) init_gl(eng);
    else if (cmd == APP_CMD_TERM_WINDOW) eng->display = NULL;
}

void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng; state->onAppCmd = handle_cmd; eng.app = state;
    while (1) {
        int events; struct android_poll_source* source;
        while (ALooper_pollOnce(eng.display ? 0 : -1, 0, &events, (void**)&source) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        if (eng.display) draw(&eng);
    }
}

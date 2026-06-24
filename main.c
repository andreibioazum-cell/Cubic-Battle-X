#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

// Математика матриц
void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = f / aspect; m[5] = f;
    m[10] = (far + near) / (near - far); m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

void mat4_rotate(float* m, float angle) {
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = cosf(angle); m[2] = sinf(angle);
    m[5] = 1.0f; m[8] = -sinf(angle);
    m[10] = cosf(angle); m[15] = 1.0f;
}

struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint prog; GLint mvp_loc; float angle;
};

static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    EGLConfig config; EGLint num;
    eglChooseConfig(eng->display, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &config, 1, &num);
    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    const char* vs = "attribute vec4 p; attribute vec4 c; varying vec4 v; uniform mat4 m; void main(){gl_Position=m*p; v=c;}";
    const char* fs = "precision mediump float; varying vec4 v; void main(){gl_FragColor=v;}";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->prog = glCreateProgram(); glAttachShader(eng->prog,vsh); glAttachShader(eng->prog,fsh); glLinkProgram(eng->prog);
    eng->mvp_loc = glGetUniformLocation(eng->prog, "m");
    glEnable(GL_DEPTH_TEST);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;
    glClearColor(0.1f, 0.2f, 0.3f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog);

    float vertices[] = {
        -0.5,-0.5,0.5, 1,0,0,  0.5,-0.5,0.5, 0,1,0,  0.5,0.5,0.5, 0,0,1, -0.5,0.5,0.5, 1,1,0,
        -0.5,-0.5,-0.5, 1,0,1, 0.5,-0.5,-0.5, 0,1,1,  0.5,0.5,-0.5, 1,1,1, -0.5,0.5,-0.5, 0,0,0
    };
    unsigned short indices[] = {0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4};

    float proj[16], model[16], mvp[16];
    mat4_perspective(proj, 1.0f, (float)ANativeWindow_getWidth(eng->app->window)/ANativeWindow_getHeight(eng->app->window), 0.1f, 10.0f);
    mat4_rotate(model, eng->angle);
    model[14] = -3.0f; // Дистанция

    // Перемножение mvp = proj * model
    for(int i=0; i<4; i++) for(int j=0; j<4; j++) {
        mvp[i*4+j] = 0;
        for(int k=0; k<4; k++) mvp[i*4+j] += proj[i*4+k] * model[k*4+j];
    }

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, vertices); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &vertices[3]); glEnableVertexAttribArray(1);
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

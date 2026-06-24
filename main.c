#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

typedef struct { float m[16]; } mat4;

mat4 mat4_perspective(float fov, float aspect, float near, float far) {
    mat4 res = {0};
    float f = 1.0f / tanf(fov * 0.5f);
    res.m[0] = f / aspect; res.m[5] = f;
    res.m[10] = (far + near) / (near - far); res.m[11] = -1.0f;
    res.m[14] = (2.0f * far * near) / (near - far);
    return res;
}

mat4 mat4_rotation(float angle) {
    mat4 res = {0};
    res.m[0] = res.m[15] = 1.0f; res.m[5] = cosf(angle);
    res.m[6] = sinf(angle); res.m[9] = -sinf(angle); res.m[10] = cosf(angle);
    return res;
}

struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLuint vbo; GLint mvp_loc; float angle;
};

static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    EGLConfig config; EGLint num;
    eglChooseConfig(eng->display, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &config, 1, &num);
    eng->surface = eglCreateWindowSurface(eng->display, config, eng->app->window, NULL);
    eng->context = eglCreateContext(eng->display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->display, eng->surface, eng->surface, eng->context);

    const char* vs = "precision highp float; attribute vec3 p; attribute vec3 c; varying vec4 v; uniform mat4 m; void main(){gl_Position=m*vec4(p,1.0); v=vec4(c,1.0);}";
    const char* fs = "precision highp float; varying vec4 v; void main(){gl_FragColor=v;}";
    
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->program = glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh); glLinkProgram(eng->program);
    eng->mvp_loc = glGetUniformLocation(eng->program, "m");

    float verts[] = {
        -0.5,-0.5, 0.5, 1,0,0,  0.5,-0.5, 0.5, 0,1,0,  0.5, 0.5, 0.5, 0,0,1, -0.5, 0.5, 0.5, 1,1,0,
        -0.5,-0.5,-0.5, 1,0,1,  0.5,-0.5,-0.5, 0,1,1,  0.5, 0.5,-0.5, 1,1,1, -0.5, 0.5,-0.5, 0,0,0,
        -0.5, 0.5, 0.5, 1,0,0,  0.5, 0.5, 0.5, 0,1,0,  0.5, 0.5,-0.5, 0,0,1, -0.5, 0.5,-0.5, 1,1,0,
        -0.5,-0.5, 0.5, 1,0,1,  0.5,-0.5, 0.5, 0,1,1,  0.5,-0.5,-0.5, 1,1,1, -0.5,-0.5,-0.5, 0,0,0
    };
    glGenBuffers(1, &eng->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnable(GL_DEPTH_TEST);
}

static void draw(struct engine* eng) {
    if (!eng->display) return;
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    glViewport(0, 0, w, h);
    glClearColor(0.1f, 0.15f, 0.2f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    mat4 proj = mat4_perspective(1.0f, (float)w/(float)h, 0.1f, 100.0f);
    mat4 rot = mat4_rotation(eng->angle);
    rot.m[14] = -2.5f; // Отодвигаем куб

    mat4 mvp = {0};
    for(int i=0; i<4; i++) for(int j=0; j<4; j++)
        for(int k=0; k<4; k++) mvp.m[i*4+j] += proj.m[i*4+k] * rot.m[k*4+j];

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp.m);
    glBindBuffer(GL_ARRAY_BUFFER, eng->vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, (void*)(3*4)); glEnableVertexAttribArray(1);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4); glDrawArrays(GL_TRIANGLE_FAN, 4, 4);
    glDrawArrays(GL_TRIANGLE_FAN, 8, 4); glDrawArrays(GL_TRIANGLE_FAN, 12, 4);
    
    eglSwapBuffers(eng->display, eng->surface);
    eng->angle += 0.03f;
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

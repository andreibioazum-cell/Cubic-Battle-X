#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

// Простая и надежная математика матриц (Column-major)
void mat4_identity(float* m) {
    for(int i=0; i<16; i++) m[i]=0;
    m[0]=m[5]=m[10]=m[15]=1.0f;
}

void mat4_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    for(int i=0; i<16; i++) m[i]=0;
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0f;
    m[14] = (2.0f * far * near) / (near - far);
}

void mat4_rotate_y(float* m, float angle) {
    mat4_identity(m);
    m[0] = cosf(angle);
    m[2] = -sinf(angle);
    m[8] = sinf(angle);
    m[10] = cosf(angle);
}

void mat4_mul(float* res, float* a, float* b) {
    float tmp[16];
    for (int i=0; i<4; i++) {
        for (int j=0; j<4; j++) {
            tmp[i*4+j] = a[i*4+0]*b[0*4+j] + a[i*4+1]*b[1*4+j] + a[i*4+2]*b[2*4+j] + a[i*4+3]*b[3*4+j];
        }
    }
    for(int i=0; i<16; i++) res[i] = tmp[i];
}

struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLuint mvp_loc; float angle;
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

    const char* vs = "attribute vec4 p; attribute vec4 c; varying vec4 v; uniform mat4 m; void main(){gl_Position=m*p; v=c;}";
    const char* fs = "precision mediump float; varying vec4 v; void main(){gl_FragColor=v;}";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->program = glCreateProgram(); glAttachShader(eng->program,vsh); glAttachShader(eng->program,fsh); glLinkProgram(eng->program);
    eng->mvp_loc = glGetUniformLocation(eng->program, "m");
    glEnable(GL_DEPTH_TEST);
}

static void draw(struct engine* eng) {
    if(!eng->display) return;
    glClearColor(0.1f, 0.2f, 0.4f, 1.0f); 
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Очищаем и цвет, и глубину!
    glUseProgram(eng->program);

    // Вершины куба: Позиция (x,y,z) + Цвет (r,g,b)
    float v[] = {
        -0.5,-0.5, 0.5,  1,0,0,   0.5,-0.5, 0.5,  0,1,0,   0.5, 0.5, 0.5,  0,0,1,  -0.5, 0.5, 0.5,  1,1,0,
        -0.5,-0.5,-0.5,  1,0,1,   0.5,-0.5,-0.5,  0,1,1,   0.5, 0.5,-0.5,  1,1,1,  -0.5, 0.5,-0.5,  0,0,0
    };
    unsigned short ind[] = {
        0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4
    };

    float p[16], r[16], t[16], mvp[16];
    float aspect = (float)ANativeWindow_getWidth(eng->app->window) / (float)ANativeWindow_getHeight(eng->app->window);
    
    mat4_perspective(p, 1.0f, aspect, 0.1f, 10.0f);
    mat4_rotate_y(r, eng->angle);
    mat4_identity(t); t[14] = -3.0f; // Отодвигаем объект по Z

    float model[16];
    mat4_mul(model, t, r);   // Сначала вращаем, потом отодвигаем
    mat4_mul(mvp, p, model); // Умножаем на проекцию

    glUniformMatrix4fv(eng->mvp_loc, 1, GL_FALSE, mvp);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6*4, v); 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6*4, &v[3]); 
    glEnableVertexAttribArray(1);

    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, ind);
    eglSwapBuffers(eng->display, eng->surface);
    eng->angle += 0.02f; // Скорость вращения
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) init_gl(eng);
    else if (cmd == APP_CMD_TERM_WINDOW) eng->display = NULL;
}

void android_main(struct android_app* s) {
    struct engine e = {0}; s->userData = &e; e.app = s;
    s->onAppCmd = handle_cmd;
    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(e.display ? 0 : -1, 0, &ev, (void**)&src) >= 0) {
            if(src) src->process(s, src);
            if(s->destroyRequested) return;
        }
        if(e.display) draw(&e);
    }
}

#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

// Математика куба
void matrix_perspective(float* m, float fov, float aspect, float near, float far) {
    float f = 1.0f / tanf(fov * 0.5f);
    for(int i=0; i<16; i++) m[i]=0;
    m[0] = f/aspect; m[5] = f; m[10] = (far+near)/(near-far); m[11] = -1.0f; m[14] = (2.0f*far*near)/(near-far);
}

void matrix_rotate_y(float* m, float angle) {
    for(int i=0; i<16; i++) m[i]=0;
    m[0]=cosf(angle); m[2]=-sinf(angle); m[5]=1; m[8]=sinf(angle); m[10]=cosf(angle); m[15]=1;
    m[14]=-3.0f; // Отодвигаем камеру
}

struct engine {
    struct android_app* app; EGLDisplay display; EGLSurface surface; EGLContext context;
    GLuint program; GLuint mvp_loc; float angle;
};

static void init_gl(struct engine* eng) {
    eng->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->display, 0, 0);
    EGLConfig config; EGLint num;
    eglChooseConfig(eng->display, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_NONE}, &config, 1, &num);
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
    glClearColor(0.1, 0.2, 0.4, 1.0); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->program);

    float v[] = {-0.5,-0.5,0.5, 1,0,0,  0.5,-0.5,0.5, 0,1,0,  0.5,0.5,0.5, 0,0,1, -0.5,0.5,0.5, 1,1,0,
                 -0.5,-0.5,-0.5, 1,0,1, 0.5,-0.5,-0.5, 0,1,1,  0.5,0.5,-0.5, 1,1,1, -0.5,0.5,-0.5, 0,0,0};
    unsigned short ind[] = {0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4};

    float p[16], r[16], mvp[16];
    matrix_perspective(p, 1.0f, (float)ANativeWindow_getWidth(eng->app->window)/ANativeWindow_getHeight(eng->app->window), 0.1f, 10.0f);
    matrix_rotate_y(r, eng->angle);
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) { mvp[i*4+j]=0; for(int k=0;k<4;k++) mvp[i*4+j]+=p[i*4+k]*r[k*4+j]; }

    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, v); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &v[3]); glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, ind);
    eglSwapBuffers(eng->display, eng->surface);
    eng->angle += 0.03f;
}

void android_main(struct android_app* s) {
    struct engine e = {0}; s->userData = &e; e.app = s;
    s->onAppCmd = [](struct android_app* a, int32_t c){ if(c==APP_CMD_INIT_WINDOW) init_gl((struct engine*)a->userData); };
    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(e.display?0:-1, 0, &ev, (void**)&src)>=0) { if(src) src->process(s, src); if(s->destroyRequested) return; }
        if(e.display) draw(&e);
    }
}

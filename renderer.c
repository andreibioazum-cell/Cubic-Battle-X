#include "renderer.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>

void renderer_init(Renderer* r, ANativeWindow* window) {
    r->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(r->display, 0, 0);

    EGLConfig config;
    EGLint numConfigs;

    eglChooseConfig(r->display,
        (EGLint[]){
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_NONE
        },
        &config, 1, &numConfigs);

    r->surface = eglCreateWindowSurface(r->display, config, window, NULL);

    r->context = eglCreateContext(
        r->display, config, NULL,
        (EGLint[]){
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        });

    eglMakeCurrent(r->display, r->surface, r->surface, r->context);

    r->width = ANativeWindow_getWidth(window);
    r->height = ANativeWindow_getHeight(window);

    glViewport(0, 0, r->width, r->height);
}

void renderer_draw(Renderer* r, Game* game) {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Пока просто очистка
    // Дальше можно вернуть отрисовку текстур и карты

    eglSwapBuffers(r->display, r->surface);
}

void renderer_shutdown(Renderer* r) {
    eglDestroySurface(r->display, r->surface);
    eglDestroyContext(r->display, r->context);
    eglTerminate(r->display);
}

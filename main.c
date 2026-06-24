#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

void android_main(struct android_app* state) {
    // Инициализация окна
    ANativeWindow_acquire(state->window);

    // Настройка EGL (связь между системой и OpenGL)
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    EGLConfig config;
    EGLint numConfigs;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    EGLSurface surface = eglCreateWindowSurface(display, config, state->window, NULL);
    EGLContext context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(display, surface, surface, context);

    float angle = 0.0f;

    while (1) {
        // Очистка экрана (красный цвет, чтобы мы видели, что работает)
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Здесь будет твой 3D код
        // Для начала просто меняем цвет фона в цикле
        angle += 0.01f;
        if (angle > 1.0f) angle = 0.0f;
        glClearColor(angle, 0.3f, 0.3f, 1.0f);

        eglSwapBuffers(display, surface);

        // Обработка системных событий
        int events;
        struct android_poll_source* source;
        if (ALooper_pollAll(0, NULL, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(state, source);
            if (state->destroyRequested != 0) return;
        }
    }
}

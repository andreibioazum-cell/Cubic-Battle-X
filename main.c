#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define LOG_TAG "CubicBattle"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

void android_main(struct android_app* state) {
    LOGI("Запуск на Tecno Spark Go...");

    // Инициализация EGL
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    EGLConfig config;
    EGLint numConfigs;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    EGLSurface surface = eglCreateWindowSurface(display, config, state->window, NULL);
    EGLContext context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(display, surface, surface, context);

    float color = 0.0f;

    while (1) {
        int events;
        struct android_poll_source* source;
        
        // ИСПРАВЛЕНО: Используем ALooper_pollOnce вместо ALooper_pollAll
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(state, source);
            if (state->destroyRequested != 0) goto shutdown;
        }

        // Анимация цвета фона
        color += 0.01f;
        if (color > 1.0f) color = 0.0f;

        glClearColor(color, 0.2f, 0.4f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        eglSwapBuffers(display, surface);
    }

shutdown:
    LOGI("Закрытие игры");
}

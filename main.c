#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define LOG_TAG "CubicBattle"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct engine {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int32_t width;
    int32_t height;
};

// Функция инициализации графики
static int engine_init_display(struct engine* engine) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);

    EGLConfig config;
    EGLint numConfigs;
    EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    
    EGLSurface surface = eglCreateWindowSurface(display, config, engine->app->window, NULL);
    EGLContext context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        LOGI("Не удалось установить контекст OpenGL");
        return -1;
    }

    engine->display = display;
    engine->surface = surface;
    engine->context = context;
    return 0;
}

// Функция отрисовки
static void engine_draw_frame(struct engine* engine) {
    if (engine->display == NULL) return;

    // Бирюзовый цвет
    glClearColor(0.0f, 0.4f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    eglSwapBuffers(engine->display, engine->surface);
}

// Обработка системных команд
static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* engine = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window != NULL) {
                engine_init_display(engine);
                engine_draw_frame(engine);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            engine->display = NULL; // Окно закрыто, рисовать нельзя
            break;
    }
}

void android_main(struct android_app* state) {
    struct engine engine = {0};

    state->userData = &engine;
    state->onAppCmd = engine_handle_cmd;
    engine.app = state;

    while (1) {
        int events;
        struct android_poll_source* source;

        // Ждем событий (например, касаний или команд системы)
        while (ALooper_pollOnce(0, NULL, &events, (void**)&source) >= 0) {
            if (source != NULL) source->process(state, source);
            if (state->destroyRequested != 0) return;
        }

        // Если окно уже есть — рисуем постоянно
        if (engine.display != NULL) {
            engine_draw_frame(&engine);
        }
    }
}

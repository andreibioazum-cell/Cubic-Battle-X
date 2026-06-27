#include "engine.h"
#include "renderer.h"
#include "game.h"
#include "entity.h"
#include <EGL/egl.h>
#include <GLES2/gl2.h>

typedef struct {
    struct android_app* app;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int animating;
    Renderer renderer;
    Game game;
} Engine;

static void handle_cmd(struct android_app* app, int32_t cmd) {
    Engine* eng = app->userData;

    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            renderer_init(&eng->renderer, app->window);
            game_init(&eng->game);
            eng->animating = 1;
            break;

        case APP_CMD_TERM_WINDOW:
            eng->animating = 0;
            renderer_shutdown(&eng->renderer);
            break;
    }
}

static int32_t handle_input(struct android_app* app, AInputEvent* event) {
    Engine* eng = app->userData;
    return game_handle_input(&eng->game, app, event);
}

void engine_run(struct android_app* state) {
    Engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;

    eng.app = state;

    while (1) {
        int ident, events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollOnce(
            eng.animating ? 0 : -1,
            NULL, &events,
            (void**)&source)) >= 0) {

            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }

        if (eng.animating) {
            game_update(&eng.game);
            renderer_draw(&eng.renderer, &eng.game);
        }
    }
}

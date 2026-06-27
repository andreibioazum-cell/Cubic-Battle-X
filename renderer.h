#ifndef RENDERER_H
#define RENDERER_H

#include <android/native_window.h>
#include "game.h"

typedef struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int width;
    int height;
} Renderer;

void renderer_init(Renderer* r, ANativeWindow* window);
void renderer_draw(Renderer* r, Game* game);
void renderer_shutdown(Renderer* r);

#endif

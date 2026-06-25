#ifndef LOBBY_H
#define LOBBY_H
#include "utils.h"

typedef enum { STATE_LOBBY, STATE_GAME } GameState;

static inline void draw_lobby(GLint mvp_loc, GLint col_loc, int w, int h, mat4 view) {
    // Фон меню
    glUniform4f(col_loc, 0.05f, 0.05f, 0.1f, 1.0f);
    draw_quad(mvp_loc, 0, 0, w, h, 0, 0, view);

    // Кнопка "START" (Белый прямоугольник в центре)
    glUniform4f(col_loc, 1.0f, 1.0f, 1.0f, 1.0f);
    draw_quad(mvp_loc, w/2 - 200, h/2 - 60, 400, 120, 0, 0, view);
}

static inline int lobby_is_clicked(float x, float y, int w, int h) {
    return (x > w/2 - 200 && x < w/2 + 200 && y > h/2 - 60 && y < h/2 + 60);
}
#endif

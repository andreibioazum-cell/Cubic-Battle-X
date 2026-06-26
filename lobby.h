#ifndef LOBBY_H
#define LOBBY_H
#include "font.h"

typedef enum { STATE_LOBBY, STATE_GAME } GameState;

static inline void draw_lobby(GLint mvp_loc, GLint col_loc, int w, int h, mat4 view) {
    glUniform4f(col_loc, 0.05f, 0.05f, 0.1f, 1.0f);
    draw_quad(mvp_loc, 0, 0, w, h, 0, 0, view);
    glUniform4f(col_loc, 0.2f, 0.5f, 1.0f, 1.0f); // Кнопка синяя
    draw_quad(mvp_loc, w/2 - 150, h/2 - 50, 300, 100, 0, 0, view);
    glUniform4f(col_loc, 1, 1, 1, 1);
    draw_digit(mvp_loc, 0, w/2 - 20, h/2 - 20, 5, view); // Рисуем "0" как символ старта
}

static inline int lobby_check(float x, float y, int w, int h) {
    return (x > w/2 - 150 && x < w/2 + 150 && y > h/2 - 50 && y < h/2 + 50);
}
#endif

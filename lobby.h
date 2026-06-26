#ifndef LOBBY_H
#define LOBBY_H
#include "utils.h"
#include "font.h"

typedef enum { STATE_LOBBY, STATE_GAME } GameState;

static inline void draw_lobby(GLint mvp_loc, GLint col_loc, int w, int h, mat4 view) {
    glUniform4f(col_loc, 0.1f, 0.1f, 0.1f, 1.0f);
    draw_quad(mvp_loc, 0, 0, w, h, 0, 0, view); // Фон

    // Стильная кнопка (Синяя с обводкой)
    glUniform4f(col_loc, 0.2f, 0.4f, 0.8f, 1.0f);
    draw_quad(mvp_loc, w/2 - 160, h/2 - 50, 320, 100, 0, 0, view);
    
    // "Текст" (цифра 0 как символ старта для примера)
    glUniform4f(col_loc, 1, 1, 1, 1);
    draw_digit(mvp_loc, col_loc, 0, w/2 - 20, h/2 - 20, 5, view);
}

static inline int lobby_is_clicked(float x, float y, int w, int h) {
    return (x > w/2 - 160 && x < w/2 + 160 && y > h/2 - 50 && y < h/2 + 50);
}
#endif

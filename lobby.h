#ifndef LOBBY_H
#define LOBBY_H
#include "utils.h"

// Перечисление для состояний игры: Лобби или Сама Игра
typedef enum { STATE_LOBBY, STATE_GAME } GameState;

// Рисует экран лобби
static inline void draw_lobby(GLint mvp_loc, GLint col_loc, int w, int h, mat4 view) {
    // Темно-синий фон
    glUniform4f(col_loc, 0.05f, 0.05f, 0.1f, 1.0f);
    draw_quad(mvp_loc, 0, 0, w, h, 0, 0, view);

    // Большая синяя кнопка "START"
    glUniform4f(col_loc, 0.2f, 0.5f, 1.0f, 1.0f);
    draw_quad(mvp_loc, w/2 - 200, h/2 - 60, 400, 120, 0, 0, view);
}

// Проверяет, был ли клик в пределах кнопки
static inline int lobby_is_clicked(float x, float y, int w, int h) {
    float btn_x_start = w/2 - 200;
    float btn_x_end   = w/2 + 200;
    float btn_y_start = h/2 - 60;
    float btn_y_end   = h/2 + 60;
    
    return (x > btn_x_start && x < btn_x_end && y > btn_y_start && y < btn_y_end);
}
#endif

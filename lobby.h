#ifndef LOBBY_H
#define LOBBY_H
#include "utils.h"
#include "font_renderer.h"

typedef enum { STATE_LOBBY, STATE_GAME } GameState;

static inline void draw_lobby(GLint mvp_loc, GLint col_loc, GLuint pos_attr, GLuint uv_attr, int w, int h, Font* font, mat4 view) {
    // Фон
    glUniform1i(glGetUniformLocation(eng->prog, "use_tex"), 0);
    glUniform4f(col_loc, 0.05f, 0.05f, 0.1f, 1.0f);
    draw_quad(mvp_loc, 0, 0, w, h, 0, 0, view);

    float btn_w = 300, btn_h = 100;
    float btn_x = w/2 - btn_w/2, btn_y = h/2 - btn_h/2;

    // Черная обводка
    glUniform4f(col_loc, 0, 0, 0, 1.0f);
    draw_quad(mvp_loc, btn_x - 4, btn_y - 4, btn_w + 8, btn_h + 8, 0, 0, view);

    // Фиолетовая кнопка
    glUniform4f(col_loc, 0.5f, 0.2f, 0.8f, 1.0f);
    draw_quad(mvp_loc, btn_x, btn_y, btn_w, btn_h, 0, 0, view);
    
    // Текст "PLAY"
    glUniform1i(glGetUniformLocation(eng->prog, "use_tex"), 1); // Включаем текстуру для шрифта
    glUniform4f(col_loc, 1, 1, 1, 1.0f); // Белый цвет текста
    glBindTexture(GL_TEXTURE_2D, font->tex_id);
    
    float text_x = w/2 - 60; // Центрируем текст вручную
    float text_y = h/2 + 15;
    font_draw_text(pos_attr, uv_attr, font, "PLAY", &text_x, &text_y);
}

static inline int lobby_is_clicked(float x, float y, int w, int h) {
    float btn_w = 300, btn_h = 100;
    float btn_x_start = w/2 - btn_w/2, btn_x_end = w/2 + btn_w/2;
    float btn_y_start = h/2 - btn_h/2, btn_y_end = h/2 + btn_h/2;
    return (x > btn_x_start && x < btn_x_end && y > btn_y_start && y < btn_y_end);
}
#endif

#ifndef LOBBY_H
#define LOBBY_H
#include "utils.h"
#include "font_renderer.h"

typedef enum { STATE_LOBBY, STATE_GAME } GameState;

static inline void draw_lobby(GLint mvp_loc, GLint col_loc, GLint use_tex_loc, GLuint p_loc, GLuint uv_loc, int w, int h, Font* font, mat4 view) {
    glUniform1i(use_tex_loc, 0);
    glUniform4f(col_loc, 0.05f, 0.05f, 0.1f, 1.0f);
    draw_quad(mvp_loc, p_loc, uv_loc, 0, 0, (float)w, (float)h, 0, 0, view);

    float btn_w = 400, btn_h = 120;
    float bx = (float)w/2.0f - btn_w/2.0f;
    float by = (float)h/2.0f - btn_h/2.0f;

    glUniform4f(col_loc, 0, 0, 0, 1.0f);
    draw_quad(mvp_loc, p_loc, uv_loc, bx - 5, by - 5, btn_w + 10, btn_h + 10, 0, 0, view);
    glUniform4f(col_loc, 0.5f, 0.0f, 0.7f, 1.0f);
    draw_quad(mvp_loc, p_loc, uv_loc, bx, by, btn_w, btn_h, 0, 0, view);
    
    glUniform1i(use_tex_loc, 2);
    glUniform4f(col_loc, 1.0f, 1.0f, 1.0f, 1.0f);
    font_draw_text(p_loc, uv_loc, font, "PLAY", (float)w/2.0f - 60, (float)h/2.0f + 20);
}

static inline int lobby_is_clicked(float x, float y, int w, int h) {
    float btn_w = 400, btn_h = 120;
    float bx = (float)w/2.0f - btn_w/2.0f;
    float by = (float)h/2.0f - btn_h/2.0f;
    return (x > bx && x < bx + btn_w && y > by && y < by + btn_h);
}
#endif

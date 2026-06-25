#ifndef UI_H
#define UI_H
#include "utils.h"

typedef struct {
    float sx, sy, cx, cy;
    int active;
} Joystick;

static inline void ui_draw_circle(GLint mvp_loc, GLint col_loc, float x, float y, float r, float thick, mat4 m) {
    float v[404]; // Для 100 сегментов
    if (thick > 0) {
        for(int i=0; i<=100; i++) {
            float a = i * 2.0f * M_PI / 100.0f;
            v[i*4] = x + cosf(a)*r; v[i*4+1] = y + sinf(a)*r;
            v[i*4+2] = x + cosf(a)*(r-thick); v[i*4+3] = y + sinf(a)*(r-thick);
        }
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 202);
    } else {
        for(int i=0; i<=100; i++) {
            float a = i * 2.0f * M_PI / 100.0f;
            v[i*2] = x + cosf(a)*r; v[i*2+1] = y + sinf(a)*r;
        }
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 101);
    }
}
#endif

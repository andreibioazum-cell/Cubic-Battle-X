#ifndef UI_H
#define UI_H
#include "utils.h"

typedef struct {
    float sx, sy, cx, cy;
    int active;
} Joystick;

static inline void ui_draw_circle(GLint mvp_loc, GLint col_loc, 
                                  float x, float y, float r, 
                                  float thick, mat4 m) {
    #define SEGMENTS 100
    float v[(SEGMENTS+1)*4];
    
    glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
    
    if (thick > 0) {
        // Рисуем обводку (кольцо)
        for(int i=0; i<=SEGMENTS; i++) {
            float a = i * 2.0f * M_PI / SEGMENTS;
            float cos_a = cosf(a);
            float sin_a = sinf(a);
            // Внутренняя точка
            v[i*4] = x + cos_a * (r - thick);
            v[i*4+1] = y + sin_a * (r - thick);
            // Внешняя точка
            v[i*4+2] = x + cos_a * r;
            v[i*4+3] = y + sin_a * r;
        }
        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 4*4, v);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, 0, 4*4, &v[2]);
        glEnableVertexAttribArray(1);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, (SEGMENTS+1)*2);
    } else {
        // Рисуем залитый круг
        for(int i=0; i<=SEGMENTS; i++) {
            float a = i * 2.0f * M_PI / SEGMENTS;
            v[i*2] = x + cosf(a) * r;
            v[i*2+1] = y + sinf(a) * r;
        }
        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 2*4, v);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, SEGMENTS+1);
    }
    #undef SEGMENTS
}

#endif

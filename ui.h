#ifndef UI_H
#define UI_H
#include "utils.h"

typedef struct { float sx, sy, cx, cy; int active, pid; } Joystick;

static inline void ui_draw_circle(GLint mvp_loc, GLint p_loc, float x, float y, float r, float thick, mat4 m) {
    const int segs = 64; float v[(segs + 2) * 4];
    glEnableVertexAttribArray(p_loc);
    glDisableVertexAttribArray(1); // Отключаем текстурные координаты, они не нужны
    
    if (thick > 0) {
        for(int i=0; i<=segs; i++) {
            float a = i * 6.28318f / segs;
            v[i*4]=x+cosf(a)*r; v[i*4+1]=y+sinf(a)*r;
            v[i*4+2]=x+cosf(a)*(r-thick); v[i*4+3]=y+sinf(a)*(r-thick);
        }
        glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 16, v);
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, (segs+1)*2);
    } else {
        v[0]=x; v[1]=y;
        for(int i=0; i<=segs; i++) {
            float a = i * 6.28318f / segs;
            v[(i+1)*2]=x+cosf(a)*r; v[(i+1)*2+1]=y+sinf(a)*r;
        }
        glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 8, v);
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_FAN, 0, segs+2);
    }
    glDisableVertexAttribArray(p_loc);
}
#endif

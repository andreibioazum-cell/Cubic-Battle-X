#ifndef UI_H
#define UI_H
#include "utils.h"

typedef struct { float sx, sy, cx, cy; int active, pid; } Joystick;

static inline void ui_draw_circle(GLint mvp_loc, GLint p_loc, float x, float y, float r, mat4 m) {
    const int segs = 40; 
    float v[(segs + 2) * 2]; 
    v[0] = x; v[1] = y;
    for(int i = 0; i <= segs; i++) {
        float a = i * 6.28318f / (float)segs; 
        v[(i+1)*2] = x + cosf(a) * r; 
        v[(i+1)*2+1] = y + sinf(a) * r;
    }
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, m.m);
    glEnableVertexAttribArray(p_loc); 
    glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 0, v);
    glDrawArrays(GL_TRIANGLE_FAN, 0, segs + 2);
    glDisableVertexAttribArray(p_loc);
}
#endif

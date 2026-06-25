#ifndef UI_H
#define UI_H
#include "utils.h"

typedef struct { float sx, sy, cx, cy; int active; } Joystick;

static inline void ui_draw_circle(GLint mvp_loc, float x, float y, float r, float thick, mat4 m) {
    float v[404];
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, v);
    glDisableVertexAttribArray(1); 

    if (thick > 0) {
        for(int i=0; i<=100; i++) {
            float a = i * 2.0f * 3.14159f / 100.0f;
            v[i*4]=x+cosf(a)*r; v[i*4+1]=y+sinf(a)*r;
            v[i*4+2]=x+cosf(a)*(r-thick); v[i*4+3]=y+sinf(a)*(r-thick);
        }
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 202);
    } else {
        v[0]=x; v[1]=y;
        for(int i=0; i<=100; i++) {
            float a = i * 2.0f * 3.14159f / 100.0f;
            v[(i+1)*2]=x+cosf(a)*r; v[(i+1)*2+1]=y+sinf(a)*r;
        }
        glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 102);
    }
}
#endif

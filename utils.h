#ifndef UTILS_H
#define UTILS_H
#include <GLES2/gl2.h>
#include <math.h>

typedef struct { float m[16]; } mat4;

static inline float lerp(float a, float b, float t) { return a + t * (b - a); }

static inline void mat4_ortho(mat4* m, float l, float r, float b, float t) {
    for(int i=0; i<16; i++) m->m[i] = 0;
    m->m[0] = 2.0f/(r-l); m->m[5] = 2.0f/(t-b); m->m[10] = -1.0f;
    m->m[12] = -(r+l)/(r-l); m->m[13] = -(t+b)/(t-b); m->m[15] = 1.0f;
}

static inline void mat4_translate(mat4* m, float x, float y) {
    m->m[12] += m->m[0] * x; m->m[13] += m->m[5] * y;
}

// БЕЗОПАСНАЯ ВЕРСИЯ
static inline void draw_quad(GLint mvp_loc, GLint p_loc, GLint uv_loc, float x, float y, float w, float h, float tx, float ty, mat4 view) {
    float v[] = { x,y, 0,0, x,y+h, 0,ty, x+w,y, tx,0, x+w,y+h, tx,ty };
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, view.m);
    
    glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 16, v);
    glEnableVertexAttribArray(p_loc);
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 16, &v[2]);
    glEnableVertexAttribArray(uv_loc);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(p_loc);
    glDisableVertexAttribArray(uv_loc);
}

// БЕЗОПАСНАЯ ВЕРСИЯ
static inline void draw_quad_ext(GLint mvp_loc, GLint p_loc, GLint uv_loc, float x, float y, float w, float h, float tx, float ty, float angle, mat4 view) {
    mat4 m = view; mat4_translate(&m, x, y);
    float s = sinf(angle), c = cosf(angle);
    float r0 = m.m[0], r5 = m.m[5];
    m.m[0] = c * r0; m.m[1] = s * r5;
    m.m[4] = -s * r0; m.m[5] = c * r5;
    float v[] = { -w/2,-h/2, 0,0, -w/2,h/2, 0,ty, w/2,-h/2, tx,0, w/2,h/2, tx,ty };
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, m.m);
    
    glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 16, v);
    glEnableVertexAttribArray(p_loc);
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 16, &v[2]);
    glEnableVertexAttribArray(uv_loc);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    glDisableVertexAttribArray(p_loc);
    glDisableVertexAttribArray(uv_loc);
}
#endif

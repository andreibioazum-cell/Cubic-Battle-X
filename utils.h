#ifndef UTILS_H
#define MATH_H
#include <GLES2/gl2.h>
#include <malloc.h>
#include <math.h>

typedef struct { float m[16]; } mat4;

static inline void mat4_ortho(mat4* m, float l, float r, float b, float t) {
    for(int i=0; i<16; i++) m->m[i]=0;
    m->m[0]=2/(r-l); m->m[5]=2/(t-b); m->m[10]=-1; m->m[12]=-(r+l)/(r-l); m->m[13]=-(t+b)/(t-b); m->m[15]=1;
}

static inline void mat4_translate(mat4* m, float x, float y) {
    m->m[12] += x * m->m[0]; m->m[13] += y * m->m[5];
}

static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

// Рисование прямоугольника (общее)
static inline void draw_quad(GLint mvp_loc, GLint use_tex_loc, float x, float y, float w, float h, float tw, float th, mat4 m) {
    float v[] = { x,y, 0,0,  x+w,y, tw,0,  x,y+h, 0,th,  x+w,y+h, tw,th };
    glUniformMatrix4fv(mvp_loc, 1, 0, m.m);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 4*4, v); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, 0, 4*4, &v[2]); glEnableVertexAttribArray(1);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
#endif

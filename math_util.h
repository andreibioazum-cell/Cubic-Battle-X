#ifndef MATH_UTIL_H
#define MATH_UTIL_H
#include <math.h>

typedef struct { float m[16]; } mat4;

static inline float lerp(float a, float b, float t) { return a + t * (b - a); }

static inline void mat4_ortho(mat4* m, float l, float r, float b, float t) {
    for(int i=0; i<16; i++) m->m[i] = 0;
    m->m[0] = 2.0f/(r-l);  m->m[5] = 2.0f/(t-b); m->m[10] = -1.0f;
    m->m[12] = -(r+l)/(r-l); m->m[13] = -(t+b)/(t-b); m->m[15] = 1.0f;
}

static inline void mat4_translate(mat4* m, float x, float y) {
    m->m[12] += m->m[0] * x; m->m[13] += m->m[5] * y;
}
#endif

#ifndef MATH_H
#define MATH_H

typedef struct { float m[16]; } mat4;

static inline void mat4_ortho(mat4* m, float l, float r, float b, float t) {
    for(int i=0; i<16; i++) m->m[i]=0;
    m->m[0]=2/(r-l); m->m[5]=2/(t-b); m->m[10]=-1; m->m[12]=-(r+l)/(r-l); m->m[13]=-(t+b)/(t-b); m->m[15]=1;
}

static inline void mat4_translate(mat4* m, float x, float y) {
    m->m[12] += x * m->m[0];
    m->m[13] += y * m->m[5];
}
#endif

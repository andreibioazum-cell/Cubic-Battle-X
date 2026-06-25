#ifndef MATH_H
#define MATH_H
#include <math.h>
typedef struct { float m[16]; } mat4;
static inline void mat4_id(mat4* m) { for(int i=0; i<16; i++) m->m[i]=0; m->m[0]=m->m[5]=m->m[10]=m->m[15]=1.0f; }
static inline void mat4_perspective(mat4* m, float fov, float asp, float n, float f) {
    float s = 1.0f / tanf(fov * 0.5f); mat4_id(m);
    m->m[0]=s/asp; m->m[5]=s; m->m[10]=(f+n)/(n-f); m->m[11]=-1.0f; m->m[14]=(2*f*n)/(n-f); m->m[15]=0;
}
static inline void mat4_mul(mat4* res, mat4* a, mat4* b) {
    mat4 t; for(int i=0; i<4; i++) for(int j=0; j<4; j++)
        t.m[i*4+j] = a->m[0*4+j]*b->m[i*4+0] + a->m[1*4+j]*b->m[i*4+1] + a->m[2*4+j]*b->m[i*4+2] + a->m[3*4+j]*b->m[i*4+3];
    *res = t;
}
#endif

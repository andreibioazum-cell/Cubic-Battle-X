#ifndef RENDERER_H
#define RENDERER_H
#include <GLES2/gl2.h>

// Шейдер с туманом и освещением
static const char* VS = "uniform mat4 m; attribute vec4 p; attribute vec3 n; varying vec3 v_n; varying float v_d; void main(){ vec4 pos=m*p; gl_Position=pos; v_n=n; v_d=length(pos.xyz); }";
static const char* FS = "precision mediump float; uniform vec4 c; uniform vec3 l; uniform int mode; varying vec3 v_n; varying float v_d; void main(){ if(mode==2){gl_FragColor=c; return;} float df=max(dot(v_n, l), 0.2); vec4 res=vec4(c.rgb*df, c.a); float fog=clamp((v_d-2.0)/18.0, 0.0, 1.0); gl_FragColor=mix(res, vec4(0.05, 0.05, 0.05, 1.0), fog); }";

float CUBE[] = { -0.5,-0.5,0.5,0,0,1, 0.5,-0.5,0.5,0,0,1, 0.5,0.5,0.5,0,0,1, -0.5,0.5,0.5,0,0,1, -0.5,-0.5,-0.5,0,0,-1, -0.5,0.5,-0.5,0,0,-1, 0.5,0.5,-0.5,0,0,-1, 0.5,-0.5,-0.5,0,0,-1, -0.5,0.5,0.5,0,1,0, 0.5,0.5,0.5,0,1,0, 0.5,0.5,-0.5,0,1,0, -0.5,0.5,-0.5,0,1,0, -0.5,-0.5,0.5,0,-1,0, -0.5,-0.5,-0.5,0,-1,0, 0.5,-0.5,-0.5,0,-1,0, 0.5,-0.5,0.5,0,-1,0, 0.5,-0.5,0.5,1,0,0, 0.5,-0.5,-0.5,1,0,0, 0.5,0.5,-0.5,1,0,0, 0.5,0.5,0.5,1,0,0, -0.5,-0.5,0.5,-1,0,0, -0.5,0.5,0.5,-1,0,0, -0.5,0.5,-0.5,-1,0,0, -0.5,-0.5,-0.5,-1,0,0 };
unsigned short IND[] = { 0,1,2,0,2,3, 4,5,6,4,6,7, 8,9,10,8,10,11, 12,13,14,12,14,15, 16,17,18,16,18,19, 20,21,22,20,22,23 };
#endif

#ifndef SHADERS_H
#define SHADERS_H
static const char* VS = "uniform mat4 m; attribute vec4 p; attribute vec3 n; varying vec3 v_n; varying float v_d; void main(){ vec4 pos=m*p; gl_Position=pos; v_n=n; v_d=length(pos.xyz); }";
static const char* FS = "precision mediump float; uniform vec4 c; uniform vec3 l; uniform int mode; varying vec3 v_n; varying float v_d; void main(){ if(mode==2){gl_FragColor=c; return;} float df=max(dot(v_n, l), 0.3); vec4 res=vec4(c.rgb*df, 1.0); float fog=clamp((v_d-8.0)/35.0, 0.0, 1.0); gl_FragColor=mix(res, vec4(0.05, 0.05, 0.08, 1.0), fog); }";
#endif

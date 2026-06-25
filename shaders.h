#ifndef SHADERS_H
#define SHADERS_H
static const char* VS = "uniform mat4 m; attribute vec4 p; void main(){ gl_Position = m * p; }";
static const char* FS = "precision mediump float; uniform vec4 c; void main(){ gl_FragColor = c; }";
#endif

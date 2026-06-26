#ifndef SHADERS_H
#define SHADERS_H

static const char* VS = 
    "uniform mat4 m;"
    "attribute vec4 p;"
    "attribute vec2 uv;"
    "varying vec2 v_uv;"
    "void main(){"
    "  gl_Position = m * p;"
    "  v_uv = uv;"
    "}";

static const char* FS = 
    "precision mediump float;"
    "uniform vec4 c;"
    "uniform sampler2D tex;"
    "uniform int use_tex;"
    "varying vec2 v_uv;"
    "void main(){"
    "  if(use_tex == 1) {" // Режим для обычных текстур (игрок, пол)
    "    gl_FragColor = texture2D(tex, v_uv);"
    "  } else if (use_tex == 2) {" // РЕЖИМ ДЛЯ ШРИФТА (только прозрачность)
    "    gl_FragColor = vec4(c.rgb, texture2D(tex, v_uv).a);"
    "  } else {" // Режим для простых фигур (цвет)
    "    gl_FragColor = c;"
    "  }"
    "}";

#endif

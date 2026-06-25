#ifndef UTILS_H
#define UTILS_H
#include <GLES2/gl2.h>
#include "math_util.h"

static inline void draw_quad(GLint mvp_loc, float x, float y, float w, float h, float tx, float ty, mat4 view) {
    float verts[] = { 
        x, y, 0, 0, 
        x, y + h, 0, ty, 
        x + w, y, tx, 0, 
        x + w, y + h, tx, ty 
    };
    glUniformMatrix4fv(mvp_loc, 1, GL_FALSE, view.m);
    
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), verts);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), &verts[2]);
    glEnableVertexAttribArray(1);
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
#endif

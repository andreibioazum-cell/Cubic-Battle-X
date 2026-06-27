#ifndef FONT_RENDERER_H
#define FONT_RENDERER_H

#include <android/asset_manager.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include "stb_truetype.h"

#define FONT_ATLAS_WIDTH 512
#define FONT_ATLAS_HEIGHT 512

typedef struct {
    GLuint tex_id;
    stbtt_bakedchar cdata[96];
} Font;

static inline int font_init(AAssetManager* mgr, Font* font, const char* filename, float font_size) {
    AAsset* asset = AAssetManager_open(mgr, filename, AASSET_MODE_BUFFER);
    if (!asset) return 0;

    size_t asset_size = AAsset_getLength(asset);
    unsigned char* ttf_buffer = (unsigned char*)malloc(asset_size);
    AAsset_read(asset, ttf_buffer, asset_size);
    AAsset_close(asset);

    unsigned char* bitmap = (unsigned char*)malloc(FONT_ATLAS_WIDTH * FONT_ATLAS_HEIGHT);
    stbtt_BakeFontBitmap(ttf_buffer, 0, font_size, bitmap, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, 32, 96, font->cdata);
    
    glGenTextures(1, &font->tex_id);
    glBindTexture(GL_TEXTURE_2D, font->tex_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    free(bitmap);
    free(ttf_buffer);
    return 1;
}

static inline void font_draw_text(GLint p_loc, GLint uv_loc, Font* font, const char* text, float x, float y) {
    if (!font->tex_id) return;
    glEnableVertexAttribArray(p_loc);
    glEnableVertexAttribArray(uv_loc);
    glBindTexture(GL_TEXTURE_2D, font->tex_id);

    float cur_x = x;
    float cur_y = y;
    while (*text) {
        if (*text >= 32 && *text < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(font->cdata, FONT_ATLAS_WIDTH, FONT_ATLAS_HEIGHT, *text - 32, &cur_x, &cur_y, &q, 1);
            float v[] = { q.x0, q.y0, q.s0, q.t0, q.x1, q.y0, q.s1, q.t0, q.x0, q.y1, q.s0, q.t1, q.x1, q.y1, q.s1, q.t1 };
            glVertexAttribPointer(p_loc, 2, GL_FLOAT, GL_FALSE, 16, v);
            glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 16, &v[2]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }
        text++;
    }
    glDisableVertexAttribArray(p_loc);
    glDisableVertexAttribArray(uv_loc);
}
#endif

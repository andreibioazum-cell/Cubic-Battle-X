#define GL_GLEXT_PROTOTYPES
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "utils.h"
#include "shaders.h"
#include "entity.h"
#include "ui.h"
#include "lobby.h"
#include "game_logic.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, use_tex_loc;
    GLuint floor_tex, player_tex; 
    Player player; Joystick joy; GameState state;
    float camX, camY;
};

// Функция загрузки текстур (без изменений, NEAREST для пиксельности)
GLuint load_tex(struct engine* eng, const char* name) {
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!a) return 0;
    size_t s = AAsset_getLength(a);
    unsigned char* b = malloc(s); AAsset_read(a, b, s); AAsset_close(a);
    int w, h, n; unsigned char* d = stbi_load_from_memory(b, s, &w, &h, &n, 4); free(b);
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d); return t;
}

static void draw(struct engine* eng) {
    if(!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    glViewport(0,0,w,h); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mat4 view; mat4_ortho(&view, 0, w, h, 0);

    if(eng->state == STATE_LOBBY) {
        glUniform1i(eng->use_tex_loc, 0);
        draw_lobby(eng->mvp_loc, eng->col_loc, w, h, view);
    } else {
        // Логика
        if(eng->joy.active) {
            float dx=eng->joy.cx-eng->joy.sx, dy=eng->joy.cy-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
            if(d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
        } else { eng->joy.cx = eng->joy.sx; eng->joy.cy = eng->joy.sy; }

        // ПЛАВНАЯ КАМЕРА
        eng->camX = lerp(eng->camX, eng->player.x, 0.05f);
        eng->camY = lerp(eng->camY, eng->player.y, 0.05f);

        mat4 world = view; mat4_translate(&world, w/2.0f - eng->camX, h/2.0f - eng->camY);

        // 1. ПОЛ
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, 0, 0, MAP_W*TILE_SIZE, MAP_H*TILE_SIZE, 16, 9, world);

        // 2. СТЕНЫ
        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0.2, 0.2, 0.2, 1.0);
        for(int y=0; y<MAP_H; y++) for(int x=0; x<MAP_W; x++)
            if(WORLD_MAP[y][x] == 1) draw_quad(eng->mvp_loc, x*TILE_SIZE, y*TILE_SIZE, TILE_SIZE, TILE_SIZE, 0, 0, world);

        // 3. ИГРОК
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, 80, 80, 1, 1, eng->player.angle, world);

        // 4. UI
        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0, 0, 0, 1);
        ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 90, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 40, 0, view);
    }
    eglSwapBuffers(eng->disp, eng->surf);
}

// handle_input, handle_cmd, android_main остаются прежними (с учетом STATE_LOBBY)

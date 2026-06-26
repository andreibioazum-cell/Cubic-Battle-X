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
    GLuint floor_tex, player_tex; Player player; Joystick joy;
    GameState state; float camX, camY;
};

// ИСПОЛЬЗУЕТСЯ ДЛЯ ИГРОКА (Четкие пиксели)
GLuint load_texture_pixelated(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!asset) return 0;
    size_t size = AAsset_getLength(asset);
    unsigned char* buffer = malloc(size); AAsset_read(asset, buffer, size); AAsset_close(asset);
    int w, h, n; unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4); free(buffer);
    if(!data) return 0;
    GLuint tex_id; glGenTextures(1, &tex_id); glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
    return tex_id;
}

// ИСПОЛЬЗУЕТСЯ ДЛЯ ПЕСКА (Гладкое сглаживание)
GLuint load_texture_smooth(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!asset) return 0;
    size_t size = AAsset_getLength(asset);
    unsigned char* buffer = malloc(size); AAsset_read(asset, buffer, size); AAsset_close(asset);
    int w, h, n; unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4); free(buffer);
    if(!data) return 0;
    GLuint tex_id; glGenTextures(1, &tex_id); glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    return tex_id;
}

static void draw(struct engine* eng) {
    if (!eng->disp || !eng->surf || !eng->app->window) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    eng->joy.sx = 150; eng->joy.sy = h - 150;
    if (!eng->joy.active) { eng->joy.cx = eng->joy.sx; eng->joy.cy = eng->joy.sy; }

    glViewport(0, 0, w, h); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mat4 view; mat4_ortho(&view, 0, w, h, 0);

    if (eng->state == STATE_LOBBY) {
        glUniform1i(eng->use_tex_loc, 0);
        draw_lobby(eng->mvp_loc, eng->col_loc, w, h, view);
    } else {
        if (eng->joy.active) {
            float dx=eng->joy.cx-eng->joy.sx, dy=eng->joy.cy-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
            if (d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
        }
        
        // ИСПРАВЛЕНО: Камера быстрее и резче
        eng->camX = lerp(eng->camX, eng->player.x, 0.08f);
        eng->camY = lerp(eng->camY, eng->player.y, 0.08f);

        mat4 world = view; mat4_translate(&world, w/2.0f - eng->camX, h/2.0f - eng->camY);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, -10000, -10000, 20000, 20000, 200, 200, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0.2, 0.2, 0.2, 1.0f);
        for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++)
            if (WORLD_MAP[y][x] == 1) draw_quad(eng->mvp_loc, x*TILE_SIZE, y*TILE_SIZE, TILE_SIZE, TILE_SIZE, 0,0, world);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, PLAYER_SIZE, PLAYER_SIZE, 1, 1, eng->player.angle, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0,0,0,1);
        ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 80, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 30, 0, view);
    }
    eglSwapBuffers(eng->disp, eng->surf);
}

// ... handle_input() остается без изменений ...

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        if (app->window == NULL) return;
        create_map_borders();
        // ... инициализация EGL ...
        
        // ИСПРАВЛЕНО: Разные функции для разных текстур
        eng->floor_tex = load_texture_smooth(eng, "floor.png");
        eng->player_tex = load_texture_pixelated(eng, "ordinary.png");

        eng->player.x = TILE_SIZE * 2;
        eng->player.y = TILE_SIZE * 2;
        // ИСПРАВЛЕНО: Скорость снижена на 35%
        eng->player.speed = 8.0f; 
        eng->player.angle = 0.0f;
        eng->state = STATE_LOBBY;
        eng->joy.pid = -1;
    }
    // ... остальной код handle_cmd ...
}

// ... android_main() остается без изменений ...

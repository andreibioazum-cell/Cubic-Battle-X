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

// Структура движка
struct engine {
    struct android_app* app;
    EGLDisplay disp;
    EGLSurface surf;
    EGLContext ctx;
    int animating; // Флаг, что игра активна
    
    GLuint prog;
    GLint mvp_loc, col_loc, use_tex_loc;
    
    GLuint floor_tex, player_tex;
    
    Player player;
    Joystick joy;
    GameState state;
    float camX, camY;
};

// Универсальная функция загрузки текстур
GLuint load_texture(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if (!asset) return 0;

    size_t size = AAsset_getLength(asset);
    unsigned char* buffer = malloc(size);
    if (!buffer) { AAsset_close(asset); return 0; }
    
    AAsset_read(asset, buffer, size);
    AAsset_close(asset);

    int w, h, n;
    unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4);
    free(buffer);
    if (!data) return 0;

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    
    // ВАЖНО: Настройки фильтрации будут применяться ПЕРЕД отрисовкой
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    stbi_image_free(data);
    return tex_id;
}

// Отрисовка кадра
static void draw_frame(struct engine* eng) {
    if (!eng->disp) return;

    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    
    eng->joy.sx = 150;
    eng->joy.sy = h - 150;
    if (!eng->joy.active) {
        eng->joy.cx = eng->joy.sx;
        eng->joy.cy = eng->joy.sy;
    }

    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 view;
    mat4_ortho(&view, 0, w, h, 0);

    if (eng->state == STATE_LOBBY) {
        glUniform1i(eng->use_tex_loc, 0);
        draw_lobby(eng->mvp_loc, eng->col_loc, w, h, view);
    } else {
        if (eng->joy.active) {
            float dx = eng->joy.cx - eng->joy.sx, dy = eng->joy.cy - eng->joy.sy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 5.0f) entity_update_player(&eng->player, dx / d, dy / d);
        }
        
        eng->camX = lerp(eng->camX, eng->player.x, 0.08f);
        eng->camY = lerp(eng->camY, eng->player.y, 0.08f);
        mat4 world = view;
        mat4_translate(&world, w / 2.0f - eng->camX, h / 2.0f - eng->camY);
        
        // --- ПЕСОК (СГЛАЖЕННЫЙ) ---
        glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glUniform1i(eng->use_tex_loc, 1);
        draw_quad(eng->mvp_loc, -10000, -10000, 20000, 20000, 200, 200, world);

        // --- СТЕНЫ ---
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 0.2f, 0.2f, 0.2f, 1.0f);
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
            if (WORLD_MAP[y][x] == 1) draw_quad(eng->mvp_loc, x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, 0, 0, world);
        
        // --- ИГРОК (ПИКСЕЛЬНЫЙ) ---
        glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glUniform1i(eng->use_tex_loc, 1);
        draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, PLAYER_SIZE, PLAYER_SIZE, 1, 1, eng->player.angle, world);

        // --- ДЖОЙСТИК ---
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 0, 0, 0, 1.0f);
        ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 80, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 30, 0, view);
    }
    
    eglSwapBuffers(eng->disp, eng->surf);
}

// Обработка ввода
static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    // ... остальной код handle_input без изменений ...
}

// Инициализация графики
static int engine_init_display(struct engine* eng) {
    create_map_borders(); // Создаем карту до всего остального
    
    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };
    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLSurface surface;
    EGLContext context;
    EGLint format;

    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, 0, 0);
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    surface = eglCreateWindowSurface(display, config, eng->app->window, NULL);
    context = eglCreateContext(display, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) return -1;
    
    eng->disp = display;
    eng->ctx = context;
    eng->surf = surface;
    
    // --- ЗАГРУЗКА ШЕЙДЕРОВ И ТЕКСТУР ПОСЛЕ ИНИЦИАЛИЗАЦИИ EGL ---
    GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &VS, 0); glCompileShader(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &FS, 0); glCompileShader(fs);
    eng->prog = glCreateProgram(); glAttachShader(eng->prog, vs); glAttachShader(eng->prog, fs);
    glBindAttribLocation(eng->prog, 0, "p"); glBindAttribLocation(eng->prog, 1, "uv");
    glLinkProgram(eng->prog);
    
    eng->mvp_loc = glGetUniformLocation(eng->prog, "m");
    eng->col_loc = glGetUniformLocation(eng->prog, "c");
    eng->use_tex_loc = glGetUniformLocation(eng->prog, "use_tex");

    eng->floor_tex = load_texture(eng, "floor.png");
    eng->player_tex = load_texture(eng, "ordinary.png");

    glClearColor(0,0,0,1);

    return 0;
}

// Завершение работы
static void engine_term_display(struct engine* eng) {
    if (eng->disp != EGL_NO_DISPLAY) {
        eglMakeCurrent(eng->disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eng->ctx != EGL_NO_CONTEXT) eglDestroyContext(eng->disp, eng->ctx);
        if (eng->surf != EGL_NO_SURFACE) eglDestroySurface(eng->disp, eng->surf);
        eglTerminate(eng->disp);
    }
    eng->animating = 0;
    eng->disp = EGL_NO_DISPLAY;
    eng->ctx = EGL_NO_CONTEXT;
    eng->surf = EGL_NO_SURFACE;
}

// Обработка команд системы
static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (eng->app->window != NULL) {
                engine_init_display(eng);
                eng->player.x = TILE_SIZE * 2;
                eng->player.y = TILE_SIZE * 2;
                eng->player.speed = 8.0f; // Сниженная скорость
                eng->player.angle = 0.0f;
                eng->state = STATE_LOBBY;
                eng->joy.pid = -1;
                eng->animating = 1;
            }
            break;
        case APP_CMD_TERM_WINDOW:
            engine_term_display(eng);
            break;
        case APP_CMD_GAINED_FOCUS:
            eng->animating = 1;
            break;
        case APP_CMD_LOST_FOCUS:
            eng->animating = 0;
            break;
    }
}

// Главная точка входа
void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    eng.app = state;

    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollOnce(eng.animating ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) source->process(state, source);
            if (state->destroyRequested != 0) { engine_term_display(&eng); return; }
        }

        if (eng.animating) {
            draw_frame(&eng);
        }
    }
}

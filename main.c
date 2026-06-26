#define GL_GLEXT_PROTOTYPES
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Все твои хэдеры
#include "utils.h"
#include "shaders.h"
#include "entity.h"
#include "ui.h"
#include "lobby.h"
#include "game_logic.h"

// Состояние всего движка
struct engine {
    struct android_app* app;
    EGLDisplay disp;
    EGLSurface surf;
    EGLContext ctx;
    GLuint prog;
    GLint mvp_loc, col_loc, use_tex_loc;
    GLuint floor_tex, player_tex;
    Player player;
    Joystick joy;
    GameState state;
    float camX, camY;
};

// Функция загрузки текстур (со сглаживанием)
GLuint load_tex(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if (!asset) return 0;

    size_t size = AAsset_getLength(asset);
    unsigned char* buffer = malloc(size);
    AAsset_read(asset, buffer, size);
    AAsset_close(asset);

    int w, h, n;
    unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4);
    free(buffer);
    if (!data) return 0;

    GLuint tex_id;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    
    // Сглаживание (убирает "шум" песка)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    // Размытие (убирает пикселизацию)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    stbi_image_free(data);
    return tex_id;
}

// Главная функция отрисовки
static void draw(struct engine* eng) {
    if (!eng->disp || !eng->surf || !eng->app->window) return;

    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    
    // Джойстик ниже и левее
    eng->joy.sx = 150;
    eng->joy.sy = h - 150;
    if (!eng->joy.active) {
        eng->joy.cx = eng->joy.sx;
        eng->joy.cy = eng->joy.sy;
    }

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
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
        // Логика игры
        if (eng->joy.active) {
            float dx = eng->joy.cx - eng->joy.sx;
            float dy = eng->joy.cy - eng->joy.sy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 5.0f) {
                entity_update_player(&eng->player, dx / d, dy / d);
            }
        }
        
        // Плавная камера
        eng->camX = lerp(eng->camX, eng->player.x, 0.05f);
        eng->camY = lerp(eng->camY, eng->player.y, 0.05f);

        mat4 world = view;
        mat4_translate(&world, w / 2.0f - eng->camX, h / 2.0f - eng->camY);
        
        // Пол (огромный, чтобы не было черноты)
        glUniform1i(eng->use_tex_loc, 1);
        glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, -10000, -10000, 20000, 20000, 200, 200, world);

        // Стены по периметру
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 0.2f, 0.2f, 0.2f, 1.0f);
        for (int y = 0; y < MAP_H; y++) {
            for (int x = 0; x < MAP_W; x++) {
                if (WORLD_MAP[y][x] == 1) {
                    draw_quad(eng->mvp_loc, x * TILE_SIZE, y * TILE_SIZE, TILE_SIZE, TILE_SIZE, 0, 0, world);
                }
            }
        }
        
        // Игрок (большой)
        glUniform1i(eng->use_tex_loc, 1);
        glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, PLAYER_SIZE, PLAYER_SIZE, 1, 1, eng->player.angle, world);

        // Джойстик
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 0.0f, 0.0f, 0.0f, 1.0f);
        ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 80, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 30, 0, view);
    }
    
    eglSwapBuffers(eng->disp, eng->surf);
}

// Обработка ввода (касаний)
static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;

    int action = AMotionEvent_getAction(ev);
    int code = action & AMOTION_EVENT_ACTION_MASK;
    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    int id = AMotionEvent_getPointerId(ev, idx);
    float x = AMotionEvent_getX(ev, idx);
    float y = AMotionEvent_getY(ev, idx);

    if (eng->state == STATE_LOBBY) {
        if (code == AMOTION_EVENT_ACTION_DOWN) {
            if (lobby_is_clicked(x, y, ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window))) {
                eng->state = STATE_GAME;
            }
        }
    } else {
        if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float dx = x - eng->joy.sx;
            float dy = y - eng->joy.sy;
            if (sqrtf(dx * dx + dy * dy) < 180.0f && !eng->joy.active) {
                eng->joy.active = 1;
                eng->joy.pid = id;
            }
        } else if (code == AMOTION_EVENT_ACTION_MOVE) {
            for (int i = 0; i < AMotionEvent_getPointerCount(ev); i++) {
                if (AMotionEvent_getPointerId(ev, i) == eng->joy.pid) {
                    float mx = AMotionEvent_getX(ev, i);
                    float my = AMotionEvent_getY(ev, i);
                    float dx = mx - eng->joy.sx;
                    float dy = my - eng->joy.sy;
                    float d = sqrtf(dx * dx + dy * dy);
                    if (d > 80.0f) {
                        dx *= 80.0f / d;
                        dy *= 80.0f / d;
                    }
                    eng->joy.cx = eng->joy.sx + dx;
                    eng->joy.cy = eng->joy.sy + dy;
                }
            }
        } else if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (id == eng->joy.pid) {
                eng->joy.active = 0;
                eng->joy.pid = -1;
            }
        }
    }
    return 1;
}

// Обработка команд от системы Android
static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            if (app->window == NULL) return;

            // Сначала создаем карту, чтобы избежать сбоев
            create_map_borders();

            eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            eglInitialize(eng->disp, 0, 0);

            EGLConfig config;
            EGLint numConfigs;
            eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_NONE}, &config, 1, &numConfigs);
            
            eng->surf = eglCreateWindowSurface(eng->disp, config, app->window, NULL);
            eng->ctx = eglCreateContext(eng->disp, config, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});

            eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);

            GLuint vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &VS, 0);
            glCompileShader(vs);
            GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &FS, 0);
            glCompileShader(fs);

            eng->prog = glCreateProgram();
            glAttachShader(eng->prog, vs);
            glAttachShader(eng->prog, fs);
            glBindAttribLocation(eng->prog, 0, "p");
            glBindAttribLocation(eng->prog, 1, "uv");
            glLinkProgram(eng->prog);

            eng->mvp_loc = glGetUniformLocation(eng->prog, "m");
            eng->col_loc = glGetUniformLocation(eng->prog, "c");
            eng->use_tex_loc = glGetUniformLocation(eng->prog, "use_tex");

            eng->floor_tex = load_tex(eng, "floor.png");
            eng->player_tex = load_tex(eng, "ordinary.png");

            eng->player.x = TILE_SIZE * 2;
            eng->player.y = TILE_SIZE * 2;
            eng->player.speed = 12.0f;
            eng->player.angle = 0.0f;

            eng->state = STATE_LOBBY;
            eng->joy.pid = -1;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            eglMakeCurrent(eng->disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (eng->ctx != EGL_NO_CONTEXT) eglDestroyContext(eng->disp, eng->ctx);
            if (eng->surf != EGL_NO_SURFACE) eglDestroySurface(eng->disp, eng->surf);
            eglTerminate(eng->disp);
            eng->disp = EGL_NO_DISPLAY;
            eng->ctx = EGL_NO_CONTEXT;
            eng->surf = EGL_NO_SURFACE;
            break;
        }
    }
}

// Главная точка входа для Android
void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    eng.app = state;

    // Главный цикл игры с исправленной обработкой событий
    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;

        while ((ident = ALooper_pollOnce(eng.disp ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            
            if (source != NULL) {
                source->process(state, source);
            }

            if (state->destroyRequested != 0) {
                // Вызываем код завершения, чтобы корректно освободить ресурсы
                handle_cmd(state, APP_CMD_TERM_WINDOW);
                return;
            }
        }

        if (eng.disp) {
            draw(&eng);
        }
    }
}

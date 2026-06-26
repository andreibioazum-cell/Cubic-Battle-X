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

GLuint load_tex(struct engine* eng, const char* name) {
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!a) return 0;
    size_t s = AAsset_getLength(a);
    unsigned char* b = malloc(s); AAsset_read(a, b, s); AAsset_close(a);
    int w, h, n; unsigned char* d = stbi_load_from_memory(b, s, &w, &h, &n, 4); free(b);
    if(!d) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d); return t;
}

static void draw(struct engine* eng) {
    if(!eng->disp || !eng->surf || !eng->app->window) return;
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    
    eng->joy.sx = 180; eng->joy.sy = h - 180;
    if(!eng->joy.active) { eng->joy.cx = eng->joy.sx; eng->joy.cy = eng->joy.sy; }

    glViewport(0,0,w,h); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mat4 view; mat4_ortho(&view, 0, w, h, 0);

    if(eng->state == STATE_LOBBY) {
        glUniform1i(eng->use_tex_loc, 0);
        draw_lobby(eng->mvp_loc, eng->col_loc, w, h, view);
    } else {
        if(eng->joy.active) {
            float dx=eng->joy.cx-eng->joy.sx, dy=eng->joy.cy-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
            if(d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
        }
        eng->camX = lerp(eng->camX, eng->player.x, 0.05f); 
        eng->camY = lerp(eng->camY, eng->player.y, 0.05f);
        mat4 world = view; mat4_translate(&world, w/2.0f - eng->camX, h/2.0f - eng->camY);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, 0, 0, MAP_W*TILE_SIZE, MAP_H*TILE_SIZE, 16, 9, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0.2, 0.2, 0.2, 1);
        for(int y=0; y<MAP_H; y++) for(int x=0; x<MAP_W; x++)
            if(WORLD_MAP[y][x] == 1) draw_quad(eng->mvp_loc, x*TILE_SIZE, y*TILE_SIZE, TILE_SIZE, TILE_SIZE, 0, 0, world);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, 64, 64, 1, 1, eng->player.angle, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0, 0, 0, 1);
        ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 80, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 30, 0, view);
    }
    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int act = AMotionEvent_getAction(ev);
        int code = act & AMOTION_EVENT_ACTION_MASK;
        int idx = (act & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(ev, idx), y = AMotionEvent_getY(ev, idx);
        int id = AMotionEvent_getPointerId(ev, idx);

        if(eng->state == STATE_LOBBY) {
            if(code == AMOTION_EVENT_ACTION_DOWN) {
                if(lobby_is_clicked(x, y, ANativeWindow_getWidth(eng->app->window), ANativeWindow_getHeight(eng->app->window))) {
                    eng->state = STATE_GAME;
                }
            }
        } else {
            if(code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                float dx = x - eng->joy.sx, dy = y - eng->joy.sy;
                if(sqrtf(dx*dx+dy*dy) < 180.0f && !eng->joy.active) { eng->joy.active=1; eng->joy.pid=id; }
            } else if(code == AMOTION_EVENT_ACTION_MOVE) {
                for(int i=0; i<AMotionEvent_getPointerCount(ev); i++) {
                    if(AMotionEvent_getPointerId(ev, i) == eng->joy.pid) {
                        float mx=AMotionEvent_getX(ev,i), my=AMotionEvent_getY(ev,i), dx=mx-eng->joy.sx, dy=my-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
                        if(d > 80.0f) { dx*=80.0f/d; dy*=80.0f/d; }
                        eng->joy.cx=eng->joy.sx+dx; eng->joy.cy=eng->joy.sy+dy;
                    }
                }
            } else if(code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
                if(id == eng->joy.pid) { eng->joy.active=0; eng->joy.pid=-1; }
            }
        }
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if(cmd == APP_CMD_INIT_WINDOW) {
        // Убрали установку разрешения, теперь игра работает в полном разрешении экрана
        eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_NONE}, &cfg, 1, &n);
        eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
        eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
        
        GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
        eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs);
        glBindAttribLocation(eng->prog, 0, "p"); glBindAttribLocation(eng->prog, 1, "uv"); glLinkProgram(eng->prog);
        eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c"); eng->use_tex_loc=glGetUniformLocation(eng->prog,"use_tex");
        
        eng->floor_tex = load_tex(eng, "floor.png"); 
        eng->player_tex = load_tex(eng, "ordinary.png");
        
        eng->player.x = 200; eng->player.y = 200; eng->player.speed = 10.0f; 
        eng->state = STATE_LOBBY;
    } else if(cmd == APP_CMD_TERM_WINDOW) {
        // Логика завершения...
        eglMakeCurrent(eng->disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (eng->ctx != EGL_NO_CONTEXT) eglDestroyContext(eng->disp, eng->ctx);
        if (eng->surf != EGL_NO_SURFACE) eglDestroySurface(eng->disp, eng->surf);
        eglTerminate(eng->disp);
        eng->disp = EGL_NO_DISPLAY;
        eng->ctx = EGL_NO_CONTEXT;
        eng->surf = EGL_NO_SURFACE;
    }
}

void android_main(struct android_app* state) {
    struct engine eng={0}; 
    state->userData=&eng; 
    state->onAppCmd=handle_cmd; 
    state->onInputEvent=handle_input; 
    eng.app=state;

    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(eng.disp ? 0 : -1, NULL, &ev, (void**)&src) >= 0) {
            if(src) src->process(state, src); 
            if(state->destroyRequested) return;
        }
        if(eng.disp) draw(&eng);
    }
}

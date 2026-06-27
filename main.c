#define GL_GLEXT_PROTOTYPES
#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <math.h>

#include "stb_image.h"
#include "font_renderer.h"
#include "utils.h"
#include "shaders.h"
#include "entity.h"
#include "ui.h"
#include "lobby.h"
#include "game_logic.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    int animating;
    GLuint prog; GLint mvp_loc, col_loc, use_tex_loc, p_loc, uv_loc;
    GLuint floor_tex, player_tex;
    Player player; Joystick joy; GameState state; Font main_font;
    float camX, camY;
};

GLuint load_texture_smooth(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER); if (!asset) return 0;
    size_t size = AAsset_getLength(asset); unsigned char* buffer = malloc(size); AAsset_read(asset, buffer, size); AAsset_close(asset);
    int w, h, n; unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4); free(buffer); if (!data) return 0;
    GLuint tex_id; glGenTextures(1, &tex_id); glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data); glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data); return tex_id;
}

GLuint load_texture_pixelated(struct engine* eng, const char* name) {
    AAsset* asset = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER); if (!asset) return 0;
    size_t size = AAsset_getLength(asset); unsigned char* buffer = malloc(size); AAsset_read(asset, buffer, size); AAsset_close(asset);
    int w, h, n; unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4); free(buffer); if (!data) return 0;
    GLuint tex_id; glGenTextures(1, &tex_id); glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data); return tex_id;
}

static void draw_frame(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    eng->joy.sx = 150; eng->joy.sy = h - 150;
    if (!eng->joy.active) { eng->joy.cx = eng->joy.sx; eng->joy.cy = eng->joy.sy; }

    glViewport(0, 0, w, h); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    mat4 view; mat4_ortho(&view, 0, w, h, 0);

    if (eng->state == STATE_LOBBY) {
        draw_lobby(eng->mvp_loc, eng->col_loc, eng->use_tex_loc, eng->p_loc, eng->uv_loc, w, h, &eng->main_font, view);
    } else {
        if (eng->joy.active) {
            float dx=eng->joy.cx-eng->joy.sx, dy=eng->joy.cy-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
            if (d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
        }
        eng->camX = lerp(eng->camX, eng->player.x, 0.08f); eng->camY = lerp(eng->camY, eng->player.y, 0.08f);
        mat4 world = view; mat4_translate(&world, w/2.0f - eng->camX, h/2.0f - eng->camY);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, eng->p_loc, eng->uv_loc, -10000, -10000, 20000, 20000, 200, 200, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0.2f, 0.2f, 0.2f, 1.0f);
        for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++)
            if (WORLD_MAP[y][x] == 1) draw_quad(eng->mvp_loc, eng->p_loc, eng->uv_loc, x*TILE_SIZE, y*TILE_SIZE, TILE_SIZE, TILE_SIZE, 0,0, world);
        
        glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->player_tex);
        draw_quad_ext(eng->mvp_loc, eng->p_loc, eng->uv_loc, eng->player.x, eng->player.y, PLAYER_SIZE, PLAYER_SIZE, 1, 1, eng->player.angle, world);

        glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0,0,0,1);
        ui_draw_circle(eng->mvp_loc, eng->p_loc, eng->joy.sx, eng->joy.sy, 80, 4, view);
        ui_draw_circle(eng->mvp_loc, eng->p_loc, eng->joy.cx, eng->joy.cy, 30, 0, view);
    }
    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int action = AMotionEvent_getAction(ev); int code = action & AMOTION_EVENT_ACTION_MASK;
    int idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    int id = AMotionEvent_getPointerId(ev, idx);
    float x = AMotionEvent_getX(ev, idx), y = AMotionEvent_getY(ev, idx);
    if (eng->state == STATE_LOBBY) {
        if (code == AMOTION_EVENT_ACTION_DOWN && lobby_is_clicked(x, y, ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window))) {
            eng->state = STATE_GAME; return 1;
        }
    } else {
        if (code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            float dx=x-eng->joy.sx, dy=y-eng->joy.sy;
            if (sqrtf(dx*dx+dy*dy)<180.0f && !eng->joy.active) { eng->joy.active=1; eng->joy.pid=id; }
        } else if (code == AMOTION_EVENT_ACTION_MOVE) {
            for (int i=0; i<AMotionEvent_getPointerCount(ev); i++) {
                if (AMotionEvent_getPointerId(ev, i) == eng->joy.pid) {
                    float mx=AMotionEvent_getX(ev,i), my=AMotionEvent_getY(ev,i), dx=mx-eng->joy.sx, dy=my-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
                    if (d > 80.0f) { dx*=80.0f/d; dy*=80.0f/d; }
                    eng->joy.cx=eng->joy.sx+dx; eng->joy.cy=eng->joy.sy+dy;
                }
            }
        } else if (code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
            if (id == eng->joy.pid) { eng->joy.active=0; eng->joy.pid=-1; }
        }
    }
    return 1;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            if (app->window == NULL) return; create_map_borders();
            eng->disp=eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp,0,0);
            EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp,(EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_NONE},&cfg,1,&n);
            eng->surf=eglCreateWindowSurface(eng->disp,cfg,app->window,NULL);
            eng->ctx=eglCreateContext(eng->disp,cfg,NULL,(EGLint[]){EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE});
            eglMakeCurrent(eng->disp,eng->surf,eng->surf,eng->ctx);

            GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
            GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
            eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs);
            eng->p_loc = 0; eng->uv_loc = 1;
            glBindAttribLocation(eng->prog, eng->p_loc, "p"); glBindAttribLocation(eng->prog, eng->uv_loc, "uv");
            glLinkProgram(eng->prog);
            
            eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c");
            eng->use_tex_loc=glGetUniformLocation(eng->prog,"use_tex");

            font_init(app->activity->assetManager, &eng->main_font, "Roboto-Regular.ttf", 48.0f);
            eng->floor_tex = load_texture_smooth(eng, "floor.png");
            eng->player_tex = load_texture_pixelated(eng, "ordinary.png");

            eng->player.x=TILE_SIZE*2; eng->player.y=TILE_SIZE*2; eng->player.speed=8.0f; eng->player.angle=0.0f;
            eng->state=STATE_LOBBY; eng->joy.pid=-1; eng->animating=1;
            break;
        }
        case APP_CMD_TERM_WINDOW: eng->animating=0; break;
        case APP_CMD_GAINED_FOCUS: eng->animating=1; break;
        case APP_CMD_LOST_FOCUS: eng->animating=0; break;
    }
}

void android_main(struct android_app* state) {
    struct engine eng={0}; state->userData=&eng;
    state->onAppCmd=handle_cmd; state->onInputEvent=handle_input;
    eng.app=state;

    while (1) {
        int ident, events; struct android_poll_source* source;
        while ((ident = ALooper_pollOnce(eng.animating ? 0 : -1, NULL, &events, (void**)&source)) >= 0) {
            if (source) source->process(state, source);
            if (state->destroyRequested) return;
        }
        if (eng.animating) draw_frame(&eng);
    }
}

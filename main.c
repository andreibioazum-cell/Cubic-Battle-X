#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#define STB_IMAGE_IMPLEMENTATION
#include "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
#include "utils.h"
#include "shaders.h"
#include "entity.h"
#include "ui.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, use_tex_loc;
    GLuint floor_tex; Player player; Joystick joy;
    float camX, camY;
};

// Загрузка текстуры пола
GLuint load_tex(struct engine* eng, const char* name) {
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!a) return 0;
    size_t s = AAsset_getLength(a);
    unsigned char* b = malloc(s); AAsset_read(a, b, s); AAsset_close(a);
    int w, h, n; unsigned char* d = stbi_load_from_memory(b, s, &w, &h, &n, 4); free(b);
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    glGenerateMipmap(GL_TEXTURE_2D); stbi_image_free(d); return t;
}

static void draw(struct engine* eng) {
    if(!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    if(eng->joy.active) {
        float dx = eng->joy.cx - eng->joy.sx, dy = eng->joy.cy - eng->joy.sy;
        float d = sqrtf(dx*dx + dy*dy);
        if(d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
    }
    eng->camX = lerp(eng->camX, eng->player.x, 0.18f);
    eng->camY = lerp(eng->camY, eng->player.y, 0.18f);

    glViewport(0,0,w,h); glClearColor(0.2,0.2,0.2,1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 view; mat4_ortho(&view, 0, w, h, 0); mat4_translate(&view, w/2 - eng->camX, h/2 - eng->camY);
    
    // РИСУЕМ ПОЛ (Тайлинг 20x20)
    glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
    draw_quad(eng->mvp_loc, eng->use_tex_loc, -1000, -1000, 2000, 2000, 20.0f, 20.0f, view);

    // РИСУЕМ ИГРОКА (Чёрный куб)
    glUniform1i(eng->use_tex_loc, 0); glUniform4f(eng->col_loc, 0,0,0,1);
    draw_quad(eng->mvp_loc, eng->use_tex_loc, eng->player.x-40, eng->player.y-40, 80, 80, 0,0, view);

    // UI (Джойстик)
    mat4 ui; mat4_ortho(&ui, 0, w, h, 0);
    if(eng->joy.active) {
        glUniform4f(eng->col_loc, 1,1,1,1); ui_draw_circle(eng->mvp_loc, eng->col_loc, eng->joy.sx, eng->joy.sy, 80, 6, ui);
        glUniform4f(eng->col_loc, 0,0,0,1); ui_draw_circle(eng->mvp_loc, eng->col_loc, eng->joy.cx, eng->joy.cy, 35, 0, ui);
    }
    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int a = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(ev, 0), y = AMotionEvent_getY(ev, 0);
        if(a == AMOTION_EVENT_ACTION_DOWN) { eng->joy.active=1; eng->joy.sx=eng->joy.cx=x; eng->joy.sy=eng->joy.cy=y; }
        else if(a == AMOTION_EVENT_ACTION_MOVE) {
            float dx=x-eng->joy.sx, dy=y-eng->joy.sy, d=sqrtf(dx*dx+dy*dy);
            if(d>80.0f){ dx*=80.0f/d; dy*=80.0f/d; }
            eng->joy.cx=eng->joy.sx+dx; eng->joy.cy=eng->joy.sy+dy;
        } else if(a == AMOTION_EVENT_ACTION_UP) eng->joy.active=0;
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if(cmd == APP_CMD_INIT_WINDOW) {
        eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_NONE}, &cfg, 1, &n);
        eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
        eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
        GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
        eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs); glLinkProgram(eng->prog);
        eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c"); eng->use_tex_loc=glGetUniformLocation(eng->prog,"use_tex");
        eng->floor_tex = load_tex(eng, "floor.png");
        eng->player.speed = 7.5f;
    } else if(cmd == APP_CMD_TERM_WINDOW) eng->disp = NULL;
}

void android_main(struct android_app* state) {
    struct engine eng={0}; state->userData=&eng; state->onAppCmd=handle_cmd; state->onInputEvent=handle_input; eng.app=state;
    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(eng.disp?0:-1, 0, &ev, (void**)&src)>=0) { if(src) src->process(state,src); if(state->destroyRequested) return; }
        if(eng.disp) draw(&eng);
    }
}

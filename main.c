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

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, use_tex_loc;
    GLuint floor_tex, player_tex; 
    Player player; Joystick joy;
    int joy_pointer_id; // Для фикса мультитача
    float camX, camY;
};

GLuint load_tex(struct engine* eng, const char* name) {
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, name, AASSET_MODE_BUFFER);
    if(!a) return 0;
    size_t s = AAsset_getLength(a);
    unsigned char* b = malloc(s); AAsset_read(a, b, s); AAsset_close(a);
    int w, h, n; unsigned char* d = stbi_load_from_memory(b, s, &w, &h, &n, 4); free(b);
    if(!d) return 0;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, d);
    stbi_image_free(d); return t;
}

static void draw(struct engine* eng) {
    if(!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    // Обновляем джойстик (его центр)
    if(eng->joy.active) {
        float dx = eng->joy.cx - eng->joy.sx, dy = eng->joy.cy - eng->joy.sy;
        float d = sqrtf(dx*dx + dy*dy);
        if(d > 5.0f) entity_update_player(&eng->player, dx/d, dy/d);
    } else {
        // Возвращаем джойстик в центр плавно
        eng->joy.cx = lerp(eng->joy.cx, eng->joy.sx, 0.2f);
        eng->joy.cy = lerp(eng->joy.cy, eng->joy.sy, 0.2f);
    }
    
    eng->camX = lerp(eng->camX, eng->player.x, 0.1f);
    eng->camY = lerp(eng->camY, eng->player.y, 0.1f);

    glViewport(0,0,w,h); glClearColor(0.1, 0.1, 0.1, 1); glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 view; mat4_ortho(&view, 0, w, h, 0); 
    mat4 world_view = view;
    mat4_translate(&world_view, w/2.0f - eng->camX, h/2.0f - eng->camY);

    // 1. ПОЛ
    glUniform1i(eng->use_tex_loc, 1); glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
    draw_quad(eng->mvp_loc, -3000, -3000, 6000, 6000, 60, 60, world_view);

    // 2. ИГРОК (С вращением и текстурой)
    glBindTexture(GL_TEXTURE_2D, eng->player_tex);
    draw_quad_ext(eng->mvp_loc, eng->player.x, eng->player.y, 100, 100, 1, 1, eng->player.angle, world_view);

    // 3. UI ДЖОЙСТИК (Всегда виден)
    glUniform1i(eng->use_tex_loc, 0);
    glUniform4f(eng->col_loc, 0, 0, 0, 1); // Черный
    ui_draw_circle(eng->mvp_loc, eng->joy.sx, eng->joy.sy, 100, 10, view); // Меньше радиус
    ui_draw_circle(eng->mvp_loc, eng->joy.cx, eng->joy.cy, 45, 0, view);

    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(ev);
        int code = action & AMOTION_EVENT_ACTION_MASK;
        int p_idx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int p_id = AMotionEvent_getPointerId(ev, p_idx);

        float x = AMotionEvent_getX(ev, p_idx), y = AMotionEvent_getY(ev, p_idx);

        if(code == AMOTION_EVENT_ACTION_DOWN || code == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            // Если нажали в левой части экрана и джойстик еще не захвачен другим пальцем
            if(x < 500 && !eng->joy.active) { 
                eng->joy.active = 1; 
                eng->joy_pointer_id = p_id;
                eng->joy.cx = x; eng->joy.cy = y;
            }
        } else if(code == AMOTION_EVENT_ACTION_MOVE) {
            // Обновляем только если ID пальца совпадает с тем, что начал движение
            int count = AMotionEvent_getPointerCount(ev);
            for(int i=0; i<count; i++) {
                if(AMotionEvent_getPointerId(ev, i) == eng->joy_pointer_id && eng->joy.active) {
                    float mx = AMotionEvent_getX(ev, i), my = AMotionEvent_getY(ev, i);
                    float dx = mx - eng->joy.sx, dy = my - eng->joy.sy, d = sqrtf(dx*dx + dy*dy);
                    if(d > 100.0f) { dx *= 100.0f/d; dy *= 100.0f/d; }
                    eng->joy.cx = eng->joy.sx + dx; eng->joy.cy = eng->joy.sy + dy;
                }
            }
        } else if(code == AMOTION_EVENT_ACTION_UP || code == AMOTION_EVENT_ACTION_POINTER_UP) {
            if(p_id == eng->joy_pointer_id) {
                eng->joy.active = 0;
                eng->joy_pointer_id = -1;
            }
        }
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if(cmd == APP_CMD_INIT_WINDOW) {
        ANativeWindow_setBuffersGeometry(app->window, 0, 0, WINDOW_FORMAT_RGBA_8888);
        eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_NONE}, &cfg, 1, &n);
        eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
        eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
        
        GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
        eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs);
        glBindAttribLocation(eng->prog, 0, "p"); glBindAttribLocation(eng->prog, 1, "uv");
        glLinkProgram(eng->prog);
        
        eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c"); eng->use_tex_loc=glGetUniformLocation(eng->prog,"use_tex");
        eng->floor_tex = load_tex(eng, "floor.png");
        eng->player_tex = load_tex(eng, "ordinary.png");
        
        // Настройка фиксированного джойстика
        int h = ANativeWindow_getHeight(app->window);
        eng->joy.sx = 250; eng->joy.sy = h - 250;
        eng->joy.cx = 250; eng->joy.cy = h - 250;
        eng->joy_pointer_id = -1;
        
        eng->player.speed = 10.0f;
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

#include <android_native_app_glue.h>
#include <android/asset_manager.h>
#include <EGL/egl.h>
#include <android/log.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "utils.h"
#include "shaders.h"
#include "entity.h"
#include "ui.h"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "BlackCube", __VA_ARGS__)

struct engine {
    struct android_app* app;
    EGLDisplay disp;
    EGLSurface surf;
    EGLContext ctx;
    GLuint prog;
    GLint mvp_loc, col_loc, use_tex_loc;
    GLuint floor_tex;
    Player player;
    Joystick joy;
    float camX, camY;
};

// Загрузка текстуры
GLuint load_texture(struct engine* eng, const char* name) {
    AAsset* a = AAssetManager_open(eng->app->activity->assetManager, 
                                   name, AASSET_MODE_BUFFER);
    if(!a) {
        LOGI("Failed to open asset: %s", name);
        return 0;
    }
    
    size_t size = AAsset_getLength(a);
    unsigned char* buffer = (unsigned char*)malloc(size);
    AAsset_read(a, buffer, size);
    AAsset_close(a);
    
    int w, h, n;
    unsigned char* data = stbi_load_from_memory(buffer, size, &w, &h, &n, 4);
    free(buffer);
    if(!data) {
        LOGI("Failed to load image: %s", name);
        return 0;
    }
    
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(data);
    LOGI("Texture loaded: %s (%dx%d)", name, w, h);
    return tex;
}

// Создание шейдера
GLuint create_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if(!status) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOGI("Shader error: %s", log);
    }
    return shader;
}

// Инициализация OpenGL
void init_gl(struct engine* eng) {
    eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(eng->disp, NULL, NULL);
    
    EGLConfig config;
    EGLint num_configs;
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_NONE
    };
    eglChooseConfig(eng->disp, attribs, &config, 1, &num_configs);
    
    eng->surf = eglCreateWindowSurface(eng->disp, config, eng->app->window, NULL);
    eng->ctx = eglCreateContext(eng->disp, config, NULL, 
                               (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
    
    LOGI("EGL initialized");
    
    // Шейдеры
    GLuint vs = create_shader(GL_VERTEX_SHADER, VS);
    GLuint fs = create_shader(GL_FRAGMENT_SHADER, FS);
    eng->prog = glCreateProgram();
    glAttachShader(eng->prog, vs);
    glAttachShader(eng->prog, fs);
    glLinkProgram(eng->prog);
    
    eng->mvp_loc = glGetUniformLocation(eng->prog, "m");
    eng->col_loc = glGetUniformLocation(eng->prog, "c");
    eng->use_tex_loc = glGetUniformLocation(eng->prog, "use_tex");
    
    // Загрузка текстуры
    eng->floor_tex = load_texture(eng, "floor.png");
    eng->player.speed = 7.5f;
    eng->player.x = 0;
    eng->player.y = 0;
    eng->camX = 0;
    eng->camY = 0;
    LOGI("Game initialized");
}

void draw(struct engine* eng) {
    if(!eng->disp) return;
    
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);
    
    // Обновление джойстика
    if(eng->joy.active) {
        float dx = eng->joy.cx - eng->joy.sx;
        float dy = eng->joy.cy - eng->joy.sy;
        float d = sqrtf(dx*dx + dy*dy);
        if(d > 5.0f) {
            entity_update_player(&eng->player, dx/d, dy/d);
        }
    }
    
    // Плавное следование камеры
    eng->camX = lerp(eng->camX, eng->player.x, 0.18f);
    eng->camY = lerp(eng->camY, eng->player.y, 0.18f);
    
    // Очистка экрана
    glViewport(0, 0, w, h);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Матрица камеры
    mat4 view;
    mat4_ortho(&view, 0, w, h, 0);
    mat4_translate(&view, w/2 - eng->camX, h/2 - eng->camY);
    
    // РИСУЕМ ПОЛ
    if(eng->floor_tex) {
        glUniform1i(eng->use_tex_loc, 1);
        glBindTexture(GL_TEXTURE_2D, eng->floor_tex);
        draw_quad(eng->mvp_loc, eng->use_tex_loc, 
                  -1000, -1000, 2000, 2000, 20.0f, 20.0f, view);
    } else {
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 0.3f, 0.3f, 0.3f, 1.0f);
        draw_quad(eng->mvp_loc, eng->use_tex_loc,
                  -1000, -1000, 2000, 2000, 0, 0, view);
    }
    
    // РИСУЕМ ИГРОКА (Чёрный непрозрачный)
    glUniform1i(eng->use_tex_loc, 0);
    glUniform4f(eng->col_loc, 0.0f, 0.0f, 0.0f, 1.0f);
    draw_quad(eng->mvp_loc, eng->use_tex_loc,
              eng->player.x-40, eng->player.y-40, 80, 80, 0, 0, view);
    
    // РИСУЕМ UI (Джойстик)
    mat4 ui;
    mat4_ortho(&ui, 0, w, h, 0);
    
    if(eng->joy.active) {
        // Обводка джойстика - белая, толстая (15 пикселей)
        glUniform1i(eng->use_tex_loc, 0);
        glUniform4f(eng->col_loc, 1.0f, 1.0f, 1.0f, 1.0f); // Белая, непрозрачная
        ui_draw_circle(eng->mvp_loc, eng->col_loc, 
                       eng->joy.sx, eng->joy.sy, 100, 15, ui);
        
        // Заливка джойстика - серая полупрозрачная
        glUniform4f(eng->col_loc, 0.3f, 0.3f, 0.3f, 0.5f); // Серая, полупрозрачная
        ui_draw_circle(eng->mvp_loc, eng->col_loc,
                       eng->joy.sx, eng->joy.sy, 85, 0, ui);
        
        // Стик - чёрный непрозрачный
        glUniform4f(eng->col_loc, 0.0f, 0.0f, 0.0f, 1.0f); // Чёрный, непрозрачный
        ui_draw_circle(eng->mvp_loc, eng->col_loc,
                       eng->joy.cx, eng->joy.cy, 50, 0, ui);
        
        // Обводка стика - белая тонкая
        glUniform4f(eng->col_loc, 1.0f, 1.0f, 1.0f, 0.8f);
        ui_draw_circle(eng->mvp_loc, eng->col_loc,
                       eng->joy.cx, eng->joy.cy, 50, 5, ui);
    }
    
    eglSwapBuffers(eng->disp, eng->surf);
}

// Обработка ввода
int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if(AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    
    int action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    float x = AMotionEvent_getX(ev, 0);
    float y = AMotionEvent_getY(ev, 0);
    
    switch(action) {
        case AMOTION_EVENT_ACTION_DOWN:
            eng->joy.active = 1;
            eng->joy.sx = eng->joy.cx = x;
            eng->joy.sy = eng->joy.cy = y;
            break;
            
        case AMOTION_EVENT_ACTION_MOVE: {
            float dx = x - eng->joy.sx;
            float dy = y - eng->joy.sy;
            float d = sqrtf(dx*dx + dy*dy);
            float max_dist = 150.0f; // Максимальное расстояние для стика
            if(d > max_dist) {
                dx *= max_dist / d;
                dy *= max_dist / d;
            }
            eng->joy.cx = eng->joy.sx + dx;
            eng->joy.cy = eng->joy.sy + dy;
            break;
        }
            
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            eng->joy.active = 0;
            break;
    }
    return 1;
}

// Обработка команд
void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    switch(cmd) {
        case APP_CMD_INIT_WINDOW:
            init_gl(eng);
            break;
        case APP_CMD_TERM_WINDOW:
            if(eng->disp) {
                eglMakeCurrent(eng->disp, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if(eng->surf) eglDestroySurface(eng->disp, eng->surf);
                if(eng->ctx) eglDestroyContext(eng->disp, eng->ctx);
                eglTerminate(eng->disp);
                eng->disp = NULL;
            }
            break;
    }
}

// Главный цикл
void android_main(struct android_app* state) {
    struct engine eng = {0};
    state->userData = &eng;
    state->onAppCmd = handle_cmd;
    state->onInputEvent = handle_input;
    eng.app = state;
    
    LOGI("Android main started");
    
    while(1) {
        int events;
        struct android_poll_source* source;
        while(ALooper_pollOnce(eng.disp ? 0 : -1, NULL, &events, (void**)&source) >= 0) {
            if(source) source->process(state, source);
            if(state->destroyRequested) {
                LOGI("Destroy requested");
                return;
            }
        }
        if(eng.disp) draw(&eng);
    }
}

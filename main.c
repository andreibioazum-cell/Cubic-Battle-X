#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include "math_util.h"
#include "shaders.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc;
    float px, py;   // Позиция игрока
    float cx, cy;   // Позиция камеры (плавная)
    float npx, npy; // NPC
    float jsx, jsy, jcx, jcy; int tj;
};

void draw_rect(struct engine* eng, float x, float y, float w, float h, float r, float g, float b, float a, mat4 m) {
    float v[] = { x,y, x+w,y, x,y+h, x+w,y+h };
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, m.m);
    glUniform4f(eng->col_loc, r, g, b, a);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, v);
    glEnableVertexAttribArray(0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void draw_circle(struct engine* eng, float x, float y, float radius, int filled, float r, float g, float b, float a, mat4 m) {
    float v[144];
    for(int i=0; i<72; i++) {
        v[i*2] = x + cosf(i * 5.0f * M_PI / 180.0f) * radius;
        v[i*2+1] = y + sinf(i * 5.0f * M_PI / 180.0f) * radius;
    }
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, m.m);
    glUniform4f(eng->col_loc, r, g, b, a);
    glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, v);
    glEnableVertexAttribArray(0);
    glDrawArrays(filled ? GL_TRIANGLE_FAN : GL_LINE_LOOP, 0, 72);
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);

    if (eng->tj) {
        float dx = eng->jcx - eng->jsx;
        float dy = eng->jcy - eng->jsy;
        float d = sqrtf(dx*dx + dy*dy);
        if (d > 5.0f) {
            eng->px += (dx/d) * 12.0f; // Скорость чуть выше
            eng->py += (dy/d) * 12.0f;
        }
    }

    // ПЛАВНАЯ КАМЕРА (Lerp)
    // 0.1f - коэффициент плавности (чем меньше, тем медленнее камера)
    eng->cx = lerp(eng->cx, eng->px, 0.1f);
    eng->cy = lerp(eng->cy, eng->py, 0.1f);

    glViewport(0, 0, w, h);
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 view;
    mat4_ortho(&view, 0, w, h, 0);
    mat4_translate(&view, w/2 - eng->cx, h/2 - eng->cy);

    draw_rect(eng, eng->npx - 40, eng->npy - 40, 80, 80, 0.4, 0.4, 0.4, 1.0, view);
    draw_rect(eng, eng->px - 40, eng->py - 40, 80, 80, 0, 0, 0, 1.0, view);

    // --- UI (НЕПРОЗРАЧНЫЙ И МЕЛКИЙ) ---
    mat4 ui;
    mat4_ortho(&ui, 0, w, h, 0);

    if (eng->tj) {
        glLineWidth(6.0f);
        // Основание: меньше (радиус 80) и непрозрачное (alpha 1.0)
        draw_circle(eng, eng->jsx, eng->jsy, 80, 0, 0, 0, 0, 1.0, ui);
        // Стик: меньше (радиус 35) и непрозрачный (alpha 1.0)
        draw_circle(eng, eng->jcx, eng->jcy, 35, 1, 0, 0, 0, 1.0, ui);
    }

    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int act = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        float x = AMotionEvent_getX(ev, 0);
        float y = AMotionEvent_getY(ev, 0);
        if (act == AMOTION_EVENT_ACTION_DOWN) {
            eng->tj = 1; eng->jsx = x; eng->jsy = y; eng->jcx = x; eng->jcy = y;
        } else if (act == AMOTION_EVENT_ACTION_MOVE) {
            float dx = x - eng->jsx, dy = y - eng->jsy;
            float d = sqrtf(dx*dx + dy*dy);
            if (d > 80.0f) { dx *= 80.0f/d; dy *= 80.0f/d; }
            eng->jcx = eng->jsx + dx; eng->jcy = eng->jsy + dy;
        } else if (act == AMOTION_EVENT_ACTION_UP) eng->tj = 0;
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_NONE}, &cfg, 1, &n);
        eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
        eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
        eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
        GLuint vs = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs, 1, &VS, 0); glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs, 1, &FS, 0); glCompileShader(fs);
        eng->prog = glCreateProgram(); glAttachShader(eng->prog, vs); glAttachShader(eng->prog, fs); glLinkProgram(eng->prog);
        eng->mvp_loc = glGetUniformLocation(eng->prog, "m"); eng->col_loc = glGetUniformLocation(eng->prog, "c");
        eng->px = 0; eng->py = 0; eng->cx = 0; eng->cy = 0; eng->npx = 300; eng->npy = 300;
    } else if (cmd == APP_CMD_TERM_WINDOW) eng->disp = NULL;
}

void android_main(struct android_app* state) {
    struct engine eng = {0}; state->userData = &eng; state->onAppCmd = handle_cmd; state->onInputEvent = handle_input; eng.app = state;
    while (1) {
        int ev; struct android_poll_source* src;
        while (ALooper_pollOnce(eng.disp ? 0 : -1, 0, &ev, (void**)&src) >= 0) {
            if (src) src->process(state, src);
            if (state->destroyRequested) return;
        }
        if (eng.disp) draw(&eng);
    }
}

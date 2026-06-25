#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>
#include "math_util.h"
#include "shaders.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc;
    float px, py, cx, cy;
    float npx, npy;
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

// ИСПРАВЛЕНО: Кольцо теперь без дырок справа
void draw_circle_advanced(struct engine* eng, float x, float y, float radius, float thickness, float r, float g, float b, mat4 m) {
    const int segments = 100; // 100 достаточно для гладкости
    if (thickness > 0) { 
        float v_ring[(segments + 1) * 4];
        for(int i = 0; i <= segments; i++) {
            float angle = i * (2.0f * M_PI / (float)segments);
            float cos_a = cosf(angle);
            float sin_a = sinf(angle);
            v_ring[i*4]   = x + cos_a * radius;
            v_ring[i*4+1] = y + sin_a * radius;
            v_ring[i*4+2] = x + cos_a * (radius - thickness);
            v_ring[i*4+3] = y + sin_a * (radius - thickness);
        }
        glUniformMatrix4fv(eng->mvp_loc, 1, 0, m.m);
        glUniform4f(eng->col_loc, r, g, b, 1.0);
        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, v_ring);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, (segments + 1) * 2);
    } else { 
        float v_fill[(segments + 1) * 2];
        for(int i = 0; i <= segments; i++) {
            float angle = i * (2.0f * M_PI / (float)segments);
            v_fill[i*2]   = x + cosf(angle) * radius;
            v_fill[i*2+1] = y + sinf(angle) * radius;
        }
        glUniformMatrix4fv(eng->mvp_loc, 1, 0, m.m);
        glUniform4f(eng->col_loc, r, g, b, 1.0);
        glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, v_fill);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 1);
    }
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window);
    int h = ANativeWindow_getHeight(eng->app->window);

    if (eng->tj) {
        float dx = eng->jcx - eng->jsx, dy = eng->jcy - eng->jsy;
        float d = sqrtf(dx*dx + dy*dy);
        if (d > 5.0f) {
            // СКОРОСТЬ: 7.5f (+15% от 6.5)
            eng->px += (dx/d) * 7.5f; 
            eng->py += (dy/d) * 7.5f;
        }
    }

    // КАМЕРА: возвращена плавность (0.18f - золотая середина)
    eng->cx = lerp(eng->cx, eng->px, 0.18f);
    eng->cy = lerp(eng->cy, eng->py, 0.18f);

    glViewport(0, 0, w, h);
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(eng->prog);

    mat4 view;
    mat4_ortho(&view, 0, w, h, 0);
    mat4_translate(&view, w/2 - eng->cx, h/2 - eng->cy);

    draw_rect(eng, eng->npx - 40, eng->npy - 40, 80, 80, 0.4, 0.4, 0.4, 1.0, view);
    draw_rect(eng, eng->px - 40, eng->py - 40, 80, 80, 0, 0, 0, 1.0, view);

    mat4 ui;
    mat4_ortho(&ui, 0, w, h, 0);
    if (eng->tj) {
        // Кольцо теперь целое (радиус 70, толщина 6)
        draw_circle_advanced(eng, eng->jsx, eng->jsy, 70, 6, 0, 0, 0, ui);
        draw_circle_advanced(eng, eng->jcx, eng->jcy, 30, 0, 0, 0, 0, ui);
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
            if (d > 70.0f) { dx *= 70.0f/d; dy *= 70.0f/d; }
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
        eng->px = 0; eng->py = 0; eng->cx = 0; eng->cy = 0; eng->npx = 250; eng->npy = 250;
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

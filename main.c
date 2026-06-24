#include <android/log.h>
#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "CubicBattle", __VA_ARGS__)

// --- МАТЕМАТИКА ---
typedef struct { float m[16]; } mat4;
void mat4_id(mat4* m) { for(int i=0; i<16; i++) m->m[i]=0; m->m[0]=m->m[5]=m->m[10]=m->m[15]=1.0f; }
void mat4_perspective(mat4* m, float fov, float asp, float n, float f) {
    float s = 1.0f / tanf(fov * 0.5f);
    mat4_id(m); m->m[0]=s/asp; m->m[5]=s; m->m[10]=(f+n)/(n-f); m->m[11]=-1.0f; m->m[14]=(2*f*n)/(n-f); m->m[15]=0;
}
void mat4_ortho(mat4* m, float l, float r, float b, float t, float n, float f) {
    mat4_id(m); m->m[0]=2/(r-l); m->m[5]=2/(t-b); m->m[10]=-2/(f-n); m->m[12]=-(r+l)/(r-l); m->m[13]=-(t+b)/(t-b); m->m[14]=-(f+n)/(f-n);
}
void mat4_mul(mat4* res, mat4* a, mat4* b) {
    mat4 t;
    for(int i=0; i<4; i++) for(int j=0; j<4; j++)
        t.m[i*4+j] = a->m[0*4+j]*b->m[i*4+0] + a->m[1*4+j]*b->m[i*4+1] + a->m[2*4+j]*b->m[i*4+2] + a->m[3*4+j]*b->m[i*4+3];
    *res = t;
}

// --- ДВИЖОК ---
struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc;
    float p_x, p_z, p_rot; // Игрок: X, Z и Угол поворота камеры
    float joy_sx, joy_sy, joy_cx, joy_cy; int touching_joy;
    float look_sx; int touching_look;
};

static void init_gl(struct engine* eng) {
    eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
    EGLConfig cfg; EGLint n;
    eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &cfg, 1, &n);
    eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
    eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);

    const char* vs = "uniform mat4 m; attribute vec4 p; void main(){gl_Position=m*p;}";
    const char* fs = "precision mediump float; uniform vec4 c; void main(){gl_FragColor=c;}";
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->prog = glCreateProgram(); glAttachShader(eng->prog,vsh); glAttachShader(eng->prog,fsh); glLinkProgram(eng->prog);
    eng->mvp_loc = glGetUniformLocation(eng->prog, "m"); eng->col_loc = glGetUniformLocation(eng->prog, "c");
    glEnable(GL_DEPTH_TEST);
}

void draw_box(struct engine* eng, mat4* mvp, float r, float g, float b) {
    float v[] = {-0.5,-0.5,0.5, 0.5,-0.5,0.5, 0.5,0.5,0.5, -0.5,0.5,0.5, -0.5,-0.5,-0.5, 0.5,-0.5,-0.5, 0.5,0.5,-0.5, -0.5,0.5,-0.5};
    unsigned short ind[] = {0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,0,3, 3,7,4, 1,5,6, 6,2,1, 3,2,6, 6,7,3, 4,5,1, 1,0,4};
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp->m); glUniform4f(eng->col_loc, r, g, b, 1.0f);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, v); glEnableVertexAttribArray(0);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, ind);
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    // ЛОГИКА ДВИЖЕНИЯ
    if (eng->touching_joy) {
        float dx = eng->joy_cx - eng->joy_sx, dy = eng->joy_cy - eng->joy_sy;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 5.0f) {
            float nx = dx/len, ny = dy/len;
            // Учитываем поворот камеры при движении!
            eng->p_x += (nx * cosf(eng->p_rot) - ny * sinf(eng->p_rot)) * 0.25f; // СКОРОСТЬ +
            eng->p_z += (nx * sinf(eng->p_rot) + ny * cosf(eng->p_rot)) * 0.25f;
        }
    }

    glViewport(0, 0, w, h); glClearColor(0.39f, 0.39f, 0.39f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog);

    // 3D МИР
    mat4 proj, view, rot, pv, mod, mvp;
    mat4_perspective(&proj, 0.78f, (float)w/h, 0.1f, 100.0f);
    mat4_id(&view); view.m[12]=-eng->p_x; view.m[13]=-10.0f; view.m[14]=-eng->p_z-15.0f;
    mat4_id(&rot); // Поворот камеры (Yaw + Pitch)
    float pitch = 0.6f;
    rot.m[5]=cosf(pitch); rot.m[6]=sinf(pitch); rot.m[9]=-sinf(pitch); rot.m[10]=cosf(pitch); // Pitch
    mat4 yaw; mat4_id(&yaw); yaw.m[0]=cosf(eng->p_rot); yaw.m[2]=-sinf(eng->p_rot); yaw.m[8]=sinf(eng->p_rot); yaw.m[10]=cosf(eng->p_rot);
    mat4_mul(&pv, &rot, &yaw); mat4_mul(&pv, &pv, &view); mat4_mul(&pv, &proj, &pv);

    draw_box(eng, &pv, 0.3f, 0.3f, 0.3f); // Пол (в центре 0,0,0)
    mat4_id(&mod); mod.m[12]=eng->p_x; mod.m[13]=1.0f; mod.m[14]=eng->p_z;
    mat4_mul(&mvp, &pv, &mod); draw_box(eng, &mvp, 1.0f, 0.1f, 0.1f); // Игрок

    // UI (ДЖОЙСТИК) - Рисуем поверх без теста глубины
    glDisable(GL_DEPTH_TEST);
    mat4 ortho; mat4_ortho(&ortho, 0, w, h, 0, -1, 1);
    if (eng->touching_joy) {
        mat4 j_base; mat4_id(&j_base); j_base.m[12]=eng->joy_sx; j_base.m[13]=eng->joy_sy; j_base.m[0]=120; j_base.m[5]=120;
        mat4_mul(&mvp, &ortho, &j_base); draw_box(eng, &mvp, 0.8f, 0.8f, 0.8f); // База
        mat4 j_knob; mat4_id(&j_knob); j_knob.m[12]=eng->joy_cx; j_knob.m[13]=eng->joy_cy; j_knob.m[0]=60; j_knob.m[5]=60;
        mat4_mul(&mvp, &ortho, &j_knob); draw_box(eng, &mvp, 0.1f, 0.1f, 0.1f); // Ручка
    }
    glEnable(GL_DEPTH_TEST);
    eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        int idx = (AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        float x = AMotionEvent_getX(ev, idx), y = AMotionEvent_getY(ev, idx);
        int w = ANativeWindow_getWidth(app->window);

        if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
            if (x < w/2) { eng->touching_joy=1; eng->joy_sx=x; eng->joy_sy=y; eng->joy_cx=x; eng->joy_cy=y; }
            else { eng->touching_look=1; eng->look_sx=x; }
        } else if (action == AMOTION_EVENT_ACTION_MOVE) {
            for(int i=0; i<AMotionEvent_getPointerCount(ev); i++) {
                float px = AMotionEvent_getX(ev, i);
                if (px < w/2 && eng->touching_joy) { eng->joy_cx=px; eng->joy_cy=AMotionEvent_getY(ev, i); }
                else if (eng->touching_look) { eng->p_rot += (px - eng->look_sx) * 0.01f; eng->look_sx=px; }
            }
        } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP || action == AMOTION_EVENT_ACTION_CANCEL) {
            if (x < w/2) eng->touching_joy=0; else eng->touching_look=0;
        }
        return 1;
    }
    return 0;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) init_gl(eng);
    else if (cmd == APP_CMD_TERM_WINDOW) eng->disp = NULL;
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

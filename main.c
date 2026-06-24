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
void mat4_mul(mat4* res, mat4* a, mat4* b) {
    mat4 t;
    for(int i=0; i<4; i++) for(int j=0; j<4; j++)
        t.m[i*4+j] = a->m[0*4+j]*b->m[i*4+0] + a->m[1*4+j]*b->m[i*4+1] + a->m[2*4+j]*b->m[i*4+2] + a->m[3*4+j]*b->m[i*4+3];
    *res = t;
}

// --- ДАННЫЕ ---
struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, light_loc, norm_loc;
    float p_x, p_z, p_rot; 
    float joy_sx, joy_sy, joy_cx, joy_cy; int touching_joy;
    float look_sx; int touching_look;
};

// Вершины куба с НОРМАЛЯМИ (для теней на гранях)
float cube_data[] = {
    // Позиция (x,y,z)      // Нормаль (nx,ny,nz)
    -0.5,-0.5, 0.5,  0, 0, 1,   0.5,-0.5, 0.5,  0, 0, 1,   0.5, 0.5, 0.5,  0, 0, 1,  -0.5, 0.5, 0.5,  0, 0, 1, // Перед
    -0.5,-0.5,-0.5,  0, 0,-1,   -0.5, 0.5,-0.5, 0, 0,-1,   0.5, 0.5,-0.5, 0, 0,-1,   0.5,-0.5,-0.5,  0, 0,-1, // Зад
    -0.5, 0.5, 0.5,  0, 1, 0,   0.5, 0.5, 0.5,  0, 1, 0,   0.5, 0.5,-0.5,  0, 1, 0,  -0.5, 0.5,-0.5,  0, 1, 0, // Верх
    -0.5,-0.5, 0.5,  0,-1, 0,   -0.5,-0.5,-0.5, 0,-1, 0,   0.5,-0.5,-0.5,  0,-1, 0,   0.5,-0.5, 0.5,  0,-1, 0, // Низ
     0.5,-0.5, 0.5,  1, 0, 0,   0.5,-0.5,-0.5,  1, 0, 0,   0.5, 0.5,-0.5,  1, 0, 0,   0.5, 0.5, 0.5,  1, 0, 0, // Право
    -0.5,-0.5, 0.5, -1, 0, 0,  -0.5, 0.5, 0.5, -1, 0, 0,  -0.5, 0.5,-0.5, -1, 0, 0,  -0.5,-0.5,-0.5, -1, 0, 0  // Лево
};
unsigned short cube_ind[] = { 0,1,2, 0,2,3, 4,5,6, 4,6,7, 8,9,10, 8,10,11, 12,13,14, 12,14,15, 16,17,18, 16,18,19, 20,21,22, 20,22,23 };

static void init_gl(struct engine* eng) {
    eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
    EGLConfig cfg; EGLint n;
    eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE}, &cfg, 1, &n);
    eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
    eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);

    const char* vs = 
        "uniform mat4 m; attribute vec4 p; attribute vec3 n; varying vec3 v_n; "
        "void main(){ gl_Position=m*p; v_n=n; }";
    const char* fs = 
        "precision mediump float; uniform vec4 c; uniform vec3 l_dir; varying vec3 v_n; "
        "void main(){ float diff=max(dot(v_n, l_dir), 0.3); gl_FragColor=vec4(c.rgb*diff, 1.0); }";
    
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); glShaderSource(vsh,1,&vs,0); glCompileShader(vsh);
    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fsh,1,&fs,0); glCompileShader(fsh);
    eng->prog = glCreateProgram(); glAttachShader(eng->prog,vsh); glAttachShader(eng->prog,fsh); glLinkProgram(eng->prog);
    eng->mvp_loc = glGetUniformLocation(eng->prog, "m"); 
    eng->col_loc = glGetUniformLocation(eng->prog, "c");
    eng->light_loc = glGetUniformLocation(eng->prog, "l_dir");
    eng->norm_loc = glGetAttribLocation(eng->prog, "n");
    glEnable(GL_DEPTH_TEST);
}

void draw_obj(struct engine* eng, mat4* mvp, float r, float g, float b) {
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp->m);
    glUniform4f(eng->col_loc, r, g, b, 1.0f);
    glUniform3f(eng->light_loc, 0.5f, 1.0f, 0.3f); // Направление света
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, cube_data); glEnableVertexAttribArray(0);
    glVertexAttribPointer(eng->norm_loc, 3, GL_FLOAT, 0, 6*4, &cube_data[3]); glEnableVertexAttribArray(eng->norm_loc);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, cube_ind);
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    if (eng->touching_joy) {
        float dx = eng->joy_cx - eng->joy_sx, dy = eng->joy_cy - eng->joy_sy;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 5.0f) {
            float nx = dx/len, ny = dy/len;
            eng->p_x += (nx * cosf(eng->p_rot) - ny * sinf(eng->p_rot)) * 0.15f;
            eng->p_z += (nx * sinf(eng->p_rot) + ny * cosf(eng->p_rot)) * 0.15f;
        }
    }

    glViewport(0, 0, w, h); glClearColor(0.39f, 0.39f, 0.39f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog);

    mat4 proj, view, rot, pv, mod, mvp;
    mat4_perspective(&proj, 1.0f, (float)w/h, 0.1f, 100.0f);
    
    // --- КАМЕРА (1-е ЛИЦО) ---
    mat4_id(&view); 
    view.m[12]=-eng->p_x; 
    view.m[13]=-1.6f; // Высота глаз персонажа
    view.m[14]=-eng->p_z;
    
    mat4_id(&rot); // Поворот головы влево/вправо
    rot.m[0]=cosf(eng->p_rot); rot.m[2]=sinf(eng->p_rot); 
    rot.m[8]=-sinf(eng->p_rot); rot.m[10]=cosf(eng->p_rot);
    
    mat4_mul(&pv, &rot, &view);
    mat4_mul(&pv, &proj, &pv);

    // ПОЛ (с затенением)
    mat4_id(&mod); mod.m[5]=0.1f; mod.m[0]=100.0f; mod.m[10]=100.0f;
    mat4_mul(&mvp, &pv, &mod); draw_obj(eng, &mvp, 0.3f, 0.4f, 0.3f);

    // ПРЕПЯТСТВИЕ (чтобы было видно 3D и тени)
    mat4_id(&mod); mod.m[12]=5.0f; mod.m[13]=1.0f; mod.m[14]=5.0f; mod.m[0]=2.0f; mod.m[5]=2.0f; mod.m[10]=2.0f;
    mat4_mul(&mvp, &pv, &mod); draw_obj(eng, &mvp, 0.8f, 0.2f, 0.2f);

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
                else if (eng->touching_look) { eng->p_rot += (px - eng->look_sx) * 0.005f; eng->look_sx=px; }
            }
        } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP) {
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

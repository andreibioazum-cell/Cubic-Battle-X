#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <time.h>
#include <stdio.h>
#include "math_util.h"
#include "renderer.h"
#include "font.h"

// 0-воздух, 1-камень, 2-земля, 3-трава
static char WORLD[10][10][4];

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, mode_loc;
    float x, y, z, rx, ry; 
    float jsx, jsy, jcx, jcy; int tj;
    float lsx, lsy; int tl;
    int fps; long last_t;
};

// ИСПРАВЛЕНО: Теперь 7 аргументов (добавлен float a)
void draw_box(struct engine* eng, mat4* mvp, float r, float g, float b, float a, int mode) {
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp->m);
    glUniform4f(eng->col_loc, r, g, b, a);
    glUniform1i(eng->mode_loc, mode);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, CUBE); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &CUBE[3]); glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, IND);
}

void draw_text(struct engine* eng, int val, float x, float y, mat4* ortho) {
    char buf[16]; int len = sprintf(buf, "%d", val);
    for(int i=0; i<len; i++) {
        int d = buf[i] - '0';
        for(int py=0; py<5; py++) for(int px=0; px<3; px++) {
            // Читаем битовую маску из font.h
            if((FONT_DATA[d*3 + py/2] >> (2-px)) & 1) {
                mat4 mo, mv; mat4_id(&mo); 
                mo.m[12]=x + i*45 + px*12; mo.m[13]=y + py*12; mo.m[0]=12; mo.m[5]=12;
                mat4_mul(&mv, ortho, &mo); 
                draw_box(eng, &mv, 1, 1, 0, 1.0f, 2); // Желтый текст
            }
        }
    }
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    long t = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    static int frame_count = 0;
    frame_count++;
    if(t - eng->last_t >= 1000) { eng->fps = frame_count; frame_count = 0; eng->last_t = t; }

    if (eng->tj) {
        float dx=(eng->jcx-eng->jsx)/100.0f, dz=(eng->jcy-eng->jsy)/100.0f;
        float s=sinf(eng->ry), c=cosf(eng->ry);
        eng->x += (dx*c-dz*s)*0.18f; eng->z += (dx*s+dz*c)*0.18f;
    }

    glViewport(0, 0, w, h); glClearColor(0.5f, 0.8f, 1.0f, 1.0f); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 pr, vi, rY, rX, pv, mo, mv;
    mat4_perspective(&pr, 1.0f, (float)w/h, 0.1f, 60.0f);
    mat4_id(&vi); vi.m[12]=-eng->x; vi.m[13]=-eng->y-2.2f; vi.m[14]=-eng->z;
    mat4_id(&rY); rY.m[0]=cosf(eng->ry); rY.m[2]=-sinf(eng->ry); rY.m[8]=sinf(eng->ry); rY.m[10]=cosf(eng->ry);
    mat4_id(&rX); rX.m[5]=cosf(eng->rx); rX.m[6]=sinf(eng->rx); rX.m[9]=-sinf(eng->rx); rX.m[10]=cosf(eng->rx);
    mat4_mul(&pv, &rX, &rY); mat4_mul(&pv, &pv, &vi); mat4_mul(&pv, &pr, &pv);

    // Рисуем блоки
    for(int ix=0; ix<10; ix++) for(int iz=0; iz<10; iz++) for(int iy=0; iy<3; iy++) {
        if(WORLD[ix][iz][iy] == 0) continue;
        mat4_id(&mo); mo.m[12]=ix*1.1f; mo.m[13]=iy*1.1f; mo.m[14]=iz*1.1f;
        mat4_mul(&mv, &pv, &mo);
        if(iy == 2) draw_box(eng, &mv, 0.3f, 0.7f, 0.3f, 1.0f, 0); // Трава
        else if(iy == 1) draw_box(eng, &mv, 0.5f, 0.35f, 0.2f, 1.0f, 0); // Земля
        else draw_box(eng, &mv, 0.5f, 0.5f, 0.55f, 1.0f, 0); // Камень
    }

    // HUD (Интерфейс)
    glDisable(GL_DEPTH_TEST); mat4 or; mat4_id(&or); or.m[0]=2.0f/w; or.m[5]=-2.0f/h; or.m[12]=-1; or.m[13]=1;
    if (eng->tj) {
        mat4_id(&mo); mo.m[12]=eng->jsx; mo.m[13]=eng->jsy; mo.m[0]=180; mo.m[5]=180;
        mat4_mul(&mv, &or, &mo); draw_box(eng, &mv, 1, 1, 1, 0.3f, 2); // Кольцо
        mat4_id(&mo); mo.m[12]=eng->jcx; mo.m[13]=eng->jcy; mo.m[0]=70; mo.m[5]=70;
        mat4_mul(&mv, &or, &mo); draw_box(eng, &mv, 0, 0, 0, 0.6f, 2); // Стик
    }
    // Рисуем FPS текст
    draw_text(eng, eng->fps, w - 200, 60, &or);
    
    glEnable(GL_DEPTH_TEST); eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int a = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    int i = (AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float x = AMotionEvent_getX(ev, i), y = AMotionEvent_getY(ev, i), w = ANativeWindow_getWidth(app->window);
    if (a == AMOTION_EVENT_ACTION_DOWN || a == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if (x < w/2) { eng->tj=1; eng->jsx=x; eng->jsy=y; eng->jcx=x; eng->jcy=y; }
        else { eng->tl=1; eng->lsx=x; eng->lsy=y; }
    } else if (a == AMOTION_EVENT_ACTION_MOVE) {
        for(int p=0; p<AMotionEvent_getPointerCount(ev); p++) {
            float px=AMotionEvent_getX(ev,p), py=AMotionEvent_getY(ev,p);
            if(px<w/2) { 
                float dx=px-eng->jsx, dy=py-eng->jsy, d=sqrtf(dx*dx+dy*dy); 
                if(d>80) {dx*=80/d; dy*=80/d;} 
                eng->jcx=eng->jsx+dx; eng->jcy=eng->jsy+dy; 
            } else { 
                // Неинвертированная камера (Вправо тянешь - вправо смотришь)
                eng->ry += (px-eng->lsx)*0.006f; 
                eng->rx = fmaxf(-1.4f, fminf(1.4f, eng->rx+(py-eng->lsy)*0.006f)); 
                eng->lsx=px; eng->lsy=py; 
            }
        }
    } else if (a == AMOTION_EVENT_ACTION_UP || a == AMOTION_EVENT_ACTION_POINTER_UP) { 
        if (x<w/2) eng->tj=0; else eng->tl=0; 
    }
    return 1;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng = (struct engine*)app->userData;
    if (cmd == APP_CMD_INIT_WINDOW) {
        eng->disp = eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp, 0, 0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp, (EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE}, &cfg, 1, &n);
        eng->surf = eglCreateWindowSurface(eng->disp, cfg, eng->app->window, NULL);
        eng->ctx = eglCreateContext(eng->disp, cfg, NULL, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE}); eglMakeCurrent(eng->disp, eng->surf, eng->surf, eng->ctx);
        GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
        eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs); glLinkProgram(eng->prog);
        eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c"); eng->mode_loc=glGetUniformLocation(eng->prog,"mode");
        glEnable(GL_DEPTH_TEST);
        for(int x=0;x<10;x++) for(int z=0;z<10;z++) for(int y=0;y<3;y++) WORLD[x][z][y]=1;
    } else if (cmd == APP_CMD_TERM_WINDOW) eng->disp = NULL;
}

void android_main(struct android_app* state) {
    struct engine eng = {0}; state->userData = &eng; state->onAppCmd = handle_cmd; state->onInputEvent = handle_input; eng.app = state;
    while (1) {
        int ev; struct android_poll_source* src;
        while (ALooper_pollOnce(eng.disp?0:-1, 0, &ev, (void**)&src)>=0) { if(src) src->process(state,src); if(state->destroyRequested) return; }
        if(eng.disp) draw(&eng);
    }
}

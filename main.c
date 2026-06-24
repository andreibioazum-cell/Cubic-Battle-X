#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "math_util.h"
#include "renderer.h"
#include "font.h"

// Типы блоков
#define B_EMPTY  0
#define B_WALL   1
#define B_FLOOR  2
#define B_FUSE   3  // Предохранитель (Красный)
#define B_PANEL  4  // Щиток (Синий)
#define B_EXIT   5  // Выход (Зеленый)
#define B_DOOR   6  // Закрытая дверь (Серая)

static char WORLD[16][16][3];

typedef struct {
    int fuses_found;
    int panel_done;
    int game_won;
    int msg_timer;
} GameState;

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, mode_loc;
    float x, y, z, rx, ry;
    float jsx, jsy, jcx, jcy; int tj;
    float lsx, lsy; int tl;
    int fps; long last_t;
    GameState gs;
};

// Коллизия
static int is_solid(float fx, float fz) {
    int ix = (int)(fx + 0.5f), iz = (int)(fz + 0.5f);
    if(ix < 0 || ix >= 16 || iz < 0 || iz >= 16) return 1;
    return (WORLD[ix][iz][1] == B_WALL || WORLD[ix][iz][1] == B_DOOR);
}

// Взаимодействие
static void interact(struct engine* eng) {
    int ix = (int)(eng->x + sinf(eng->ry) * 1.2f + 0.5f);
    int iz = (int)(eng->z + cosf(eng->ry) * 1.2f + 0.5f);
    if(ix < 0 || ix >= 16 || iz < 0 || iz >= 16) return;
    
    if(WORLD[ix][iz][0] == B_FUSE || WORLD[ix][iz][1] == B_FUSE) {
        WORLD[ix][iz][0] = B_FLOOR; WORLD[ix][iz][1] = B_EMPTY;
        eng->gs.fuses_found++;
        eng->gs.msg_timer = 100;
    }
    if((WORLD[ix][iz][0] == B_PANEL || WORLD[ix][iz][1] == B_PANEL) && eng->gs.fuses_found >= 3) {
        eng->gs.panel_done = 1;
        for(int i=0; i<16; i++) for(int j=0; j<16; j++) if(WORLD[i][j][1] == B_DOOR) WORLD[i][j][1] = B_EMPTY;
        eng->gs.msg_timer = 150;
    }
    if(WORLD[ix][iz][0] == B_EXIT && eng->gs.panel_done) eng->gs.game_won = 1;
}

void draw_box(struct engine* eng, mat4* mvp, float r, float g, float b, float a, int mode) {
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp->m);
    glUniform4f(eng->col_loc, r, g, b, a);
    glUniform1i(eng->mode_loc, mode);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, CUBE); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &CUBE[3]); glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, IND);
}

void draw_text(struct engine* eng, const char* str, float x, float y, mat4* ortho, float r, float g, float b) {
    float s = 10.0f; // Размер шрифта
    for(int i=0; str[i] != '\0'; i++) {
        int d = (str[i] >= '0' && str[i] <= '9') ? str[i] - '0' : -1;
        if(d < 0) { x += s * 3; continue; }
        for(int py=0; py<5; py++) for(int px=0; px<3; px++) {
            if((FONT_DATA[d][py] >> (2-px)) & 1) {
                mat4 mo, mv; mat4_id(&mo);
                mo.m[12] = x + i * (s * 5) + px * s; mo.m[13] = y + py * s; mo.m[0] = s; mo.m[5] = s;
                mat4_mul(&mv, ortho, &mo); draw_box(eng, &mv, r, g, b, 1.0f, 2);
            }
        }
    }
}

static void build_world() {
    memset(WORLD, 0, sizeof(WORLD));
    for(int i=0; i<16; i++) for(int j=0; j<16; j++) WORLD[i][j][0] = B_FLOOR;
    for(int i=0; i<16; i++) { WORLD[i][0][1] = B_WALL; WORLD[i][15][1] = B_WALL; WORLD[0][i][1] = B_WALL; WORLD[15][i][1] = B_WALL; }
    // Предохранители
    WORLD[2][13][0] = B_FUSE; WORLD[13][2][0] = B_FUSE; WORLD[13][13][0] = B_FUSE;
    // Щиток и выход
    WORLD[8][8][1] = B_PANEL; WORLD[14][14][0] = B_EXIT;
    // Стены-лабиринт
    for(int i=3; i<13; i++) { WORLD[i][6][1] = B_WALL; WORLD[6][i][1] = B_WALL; }
    WORLD[14][7][1] = B_DOOR;
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w = ANativeWindow_getWidth(eng->app->window), h = ANativeWindow_getHeight(eng->app->window);
    
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    long t = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    static int fc = 0; fc++;
    if(t - eng->last_t >= 1000) { eng->fps = fc; fc = 0; eng->last_t = t; }

    if(eng->tj && !eng->gs.game_won) {
        float dx = (eng->jcx - eng->jsx)/100.0f, dy = (eng->jcy - eng->jsy)/100.0f;
        float s = sinf(eng->ry), c = cosf(eng->ry);
        float nx = eng->x + (dx * c - dy * s) * 0.08f, nz = eng->z + (dx * s + dy * c) * 0.08f;
        if(!is_solid(nx, eng->z)) eng->x = nx;
        if(!is_solid(eng->x, nz)) eng->z = nz;
    }

    glViewport(0, 0, w, h); glClearColor(0.1f, 0.1f, 0.12f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 pr, vi, rY, rX, pv, mo, mv;
    mat4_perspective(&pr, 1.0f, (float)w/h, 0.1f, 50.0f);
    mat4_id(&vi); vi.m[12]=-eng->x; vi.m[13]=-1.5f; vi.m[14]=-eng->z;
    mat4_id(&rY); rY.m[0]=cosf(eng->ry); rY.m[2]=-sinf(eng->ry); rY.m[8]=sinf(eng->ry); rY.m[10]=cosf(eng->ry);
    mat4_id(&rX); rX.m[5]=cosf(eng->rx); rX.m[6]=sinf(eng->rx); rX.m[9]=-sinf(eng->rx); rX.m[10]=cosf(eng->rx);
    mat4_mul(&pv, &rX, &rY); mat4_mul(&pv, &pv, &vi); mat4_mul(&pv, &pr, &pv);

    for(int ix=0; ix<16; ix++) for(int iz=0; iz<16; iz++) for(int iy=0; iy<3; iy++) {
        if(WORLD[ix][iz][iy] == B_EMPTY) continue;
        mat4_id(&mo); mo.m[12]=(float)ix; mo.m[13]=(float)iy; mo.m[14]=(float)iz;
        mat4_mul(&mv, &pv, &mo);
        if(WORLD[ix][iz][iy]==B_FLOOR) draw_box(eng,&mv,0.3,0.3,0.3,1,0);
        else if(WORLD[ix][iz][iy]==B_WALL) draw_box(eng,&mv,0.4,0.4,0.45,1,0);
        else if(WORLD[ix][iz][iy]==B_FUSE) draw_box(eng,&mv,1.0,0.1,0.1,1,0);
        else if(WORLD[ix][iz][iy]==B_PANEL) draw_box(eng,&mv,0.1,0.1,1.0,1,0);
        else if(WORLD[ix][iz][iy]==B_EXIT) draw_box(eng,&mv,0.1,1.0,0.1,1,0);
        else if(WORLD[ix][iz][iy]==B_DOOR) draw_box(eng,&mv,0.5,0.3,0.1,1,0);
    }

    glDisable(GL_DEPTH_TEST); mat4 or; mat4_id(&or); or.m[0]=2.0f/w; or.m[5]=-2.0f/h; or.m[12]=-1; or.m[13]=1;
    if(eng->tj) {
        mat4_id(&mo); mo.m[12]=eng->jsx; mo.m[13]=eng->jsy; mo.m[0]=160; mo.m[5]=160;
        mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,1,1,1,0.2f,2);
        mat4_id(&mo); mo.m[12]=eng->jcx; mo.m[13]=eng->jcy; mo.m[0]=70; mo.m[5]=70;
        mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,0,0,0,0.6f,2);
    }
    // Кнопка взаимодействия
    mat4_id(&mo); mo.m[12]=w-120; mo.m[13]=h-120; mo.m[0]=150; mo.m[5]=150;
    mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,1,1,0,0.4f,2);

    char f_s[16]; sprintf(f_s, "%d", eng->fps); draw_text(eng, f_s, w-100, 40, &or, 1, 1, 0);
    char g_s[16]; sprintf(g_s, "%d", eng->gs.fuses_found); draw_text(eng, g_s, 40, 40, &or, 1, 0, 0);

    glEnable(GL_DEPTH_TEST); eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng = (struct engine*)app->userData;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int a = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
    int i = (AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float x = AMotionEvent_getX(ev, i), y = AMotionEvent_getY(ev, i), w = ANativeWindow_getWidth(app->window), h = ANativeWindow_getHeight(app->window);
    if (a == AMOTION_EVENT_ACTION_DOWN || a == AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if (x > w-200 && y > h-200) { interact(eng); return 1; }
        if (x < w/2) { eng->tj=1; eng->jsx=x; eng->jsy=y; eng->jcx=x; eng->jcy=y; }
        else { eng->tl=1; eng->lsx=x; eng->lsy=y; }
    } else if (a == AMOTION_EVENT_ACTION_MOVE) {
        for(int p=0; p<AMotionEvent_getPointerCount(ev); p++) {
            float px=AMotionEvent_getX(ev,p), py=AMotionEvent_getY(ev,p);
            if(px<w/2) { 
                float dx=px-eng->jsx, dy=py-eng->jsy, d=sqrtf(dx*dx+dy*dy); 
                if(d>70) {dx*=70/d; dy*=70/d;} 
                eng->jcx=eng->jsx+dx; eng->jcy=eng->jsy+dy; 
            } else { 
                eng->ry += (px-eng->lsx)*0.006f; // Вправо = Вправо
                eng->rx = fmaxf(-1.4f, fminf(1.4f, eng->rx+(py-eng->lsy)*0.006f)); 
                eng->lsx=px; eng->lsy=py; 
            }
        }
    } else if (a == AMOTION_EVENT_ACTION_UP || a == AMOTION_EVENT_ACTION_POINTER_UP) { if (x<w/2) eng->tj=0; else eng->tl=0; }
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
        build_world(); eng->x = 2.0f; eng->z = 2.0f;
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

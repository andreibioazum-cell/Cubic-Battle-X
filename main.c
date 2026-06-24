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
#define B_FUSE   3  // Предохранитель (красный)
#define B_PANEL  4  // Щиток (синий)
#define B_EXIT   5  // Выход (зелёный, только после щитка)
#define B_DOOR   6  // Дверь (серая, закрыта)

// Карта: 16x16 комнат, высота 3
static char WORLD[16][16][3];

// Состояние игры
typedef struct {
    int fuses_found;    // Найдено предохранителей (0-3)
    int panel_done;     // Щиток активирован
    int game_won;       // Игра пройдена
    char msg[64];       // Сообщение на экране
    int msg_timer;      // Таймер сообщения
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

// Проверка твёрдости блока
static int is_solid(float fx, float fz) {
    int ix=(int)(fx+0.5f), iz=(int)(fz+0.5f);
    if(ix<0||ix>=16||iz<0||iz>=16) return 1;
    return (WORLD[ix][iz][1]==B_WALL||WORLD[ix][iz][1]==B_DOOR);
}

// Взаимодействие с блоком рядом
static void interact(struct engine* eng) {
    int ix=(int)(eng->x+sinf(eng->ry)*1.5f+0.5f);
    int iz=(int)(eng->z+cosf(eng->ry)*1.5f+0.5f);
    if(ix<0||ix>=16||iz<0||iz>=16) return;
    char* b = &WORLD[ix][iz][0];
    // Подобрать предохранитель
    if(b[1]==B_FUSE||b[0]==B_FUSE) {
        b[0]=B_FLOOR; b[1]=B_EMPTY;
        eng->gs.fuses_found++;
        sprintf(eng->gs.msg, "Предохранитель %d/3!", eng->gs.fuses_found);
        eng->gs.msg_timer=120;
    }
    // Активировать щиток
    if((b[1]==B_PANEL||b[0]==B_PANEL) && eng->gs.fuses_found>=3 && !eng->gs.panel_done) {
        eng->gs.panel_done=1;
        sprintf(eng->gs.msg, "Свет восстановлен! Ищи выход!");
        eng->gs.msg_timer=180;
        // Открываем дверь выхода (меняем B_DOOR на B_EMPTY)
        for(int i=0;i<16;i++) for(int j=0;j<16;j++)
            if(WORLD[i][j][1]==B_DOOR) WORLD[i][j][1]=B_EMPTY;
    }
    // Выйти
    if((b[1]==B_EXIT||b[0]==B_EXIT) && eng->gs.panel_done) {
        eng->gs.game_won=1;
        sprintf(eng->gs.msg, "Ты выбрался! Станция 7 побеждена!");
        eng->gs.msg_timer=999;
    }
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
    for(int i=0; str[i]!='\0'; i++) {
        int d = -1;
        if(str[i]>='0'&&str[i]<='9') d=str[i]-'0';
        if(d<0) continue;
        for(int py=0; py<5; py++) for(int px=0; px<3; px++) {
            if((FONT_DATA[d*3+py/2]>>(2-px))&1) {
                mat4 mo, mv; mat4_id(&mo);
                mo.m[12]=x+i*14+px*4; mo.m[13]=y+py*4; mo.m[0]=4; mo.m[5]=4;
                mat4_mul(&mv, ortho, &mo); draw_box(eng, &mv, r, g, b, 1.0f, 2);
            }
        }
    }
}

static void build_world() {
    memset(WORLD, 0, sizeof(WORLD));
    // Пол везде
    for(int i=0;i<16;i++) for(int j=0;j<16;j++) WORLD[i][j][0]=B_FLOOR;

    // Внешние стены
    for(int i=0;i<16;i++) { WORLD[i][0][1]=B_WALL; WORLD[i][15][1]=B_WALL; WORLD[0][i][1]=B_WALL; WORLD[15][i][1]=B_WALL; }

    // Комната 1 (стартовая) - с запиской
    for(int i=2;i<7;i++) { WORLD[i][5][1]=B_WALL; } // Перегородка
    WORLD[4][5][1]=B_EMPTY; // Проход

    // Комната 2 (предохранитель 1)
    for(int i=7;i<12;i++) { WORLD[5][i][1]=B_WALL; }
    WORLD[5][9][1]=B_EMPTY; // Проход
    WORLD[2][8][0]=B_FUSE; // Предохранитель на полу

    // Комната 3 (предохранитель 2)
    for(int i=2;i<10;i++) { WORLD[8][i][1]=B_WALL; }
    WORLD[8][6][1]=B_EMPTY; // Проход
    WORLD[6][12][0]=B_FUSE; // Предохранитель

    // Тёмный коридор к комнате 4
    for(int i=9;i<14;i++) { WORLD[i][8][1]=B_WALL; WORLD[i][12][1]=B_WALL; }
    WORLD[9][8][1]=B_EMPTY; // Вход

    // Комната 4 (предохранитель 3 и щиток)
    WORLD[12][10][0]=B_FUSE;  // Предохранитель
    WORLD[13][10][0]=B_PANEL; // Щиток

    // Выход (за дверью, дверь открывается после щитка)
    WORLD[14][7][1]=B_DOOR;
    WORLD[14][6][0]=B_EXIT;
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w=ANativeWindow_getWidth(eng->app->window), h=ANativeWindow_getHeight(eng->app->window);

    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    long t=now.tv_sec*1000+now.tv_nsec/1000000;
    static int fc=0; fc++;
    if(t-eng->last_t>=1000) { eng->fps=fc; fc=0; eng->last_t=t; }
    if(eng->gs.msg_timer>0) eng->gs.msg_timer--;

    if(eng->tj && !eng->gs.game_won) {
        float dx=(eng->jcx-eng->jsx)/100.0f, dz=(eng->jcy-eng->jsy)/100.0f;
        float s=sinf(eng->ry), c=cosf(eng->ry);
        float nx=eng->x+(dx*c-dz*s)*0.07f, nz=eng->z+(dx*s+dz*c)*0.07f;
        if(!is_solid(nx,eng->z)) eng->x=nx;
        if(!is_solid(eng->x,nz)) eng->z=nz;
    }

    // Фон: тёмный ночной — почти чёрный
    glViewport(0,0,w,h); glClearColor(0.03f,0.03f,0.05f,1.0f); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    mat4 pr,vi,rY,rX,pv,mo,mv;
    mat4_perspective(&pr,1.0f,(float)w/h,0.1f,40.0f);
    mat4_id(&vi); vi.m[12]=-eng->x; vi.m[13]=-1.5f; vi.m[14]=-eng->z;
    mat4_id(&rY); rY.m[0]=cosf(eng->ry); rY.m[2]=-sinf(eng->ry); rY.m[8]=sinf(eng->ry); rY.m[10]=cosf(eng->ry);
    mat4_id(&rX); rX.m[5]=cosf(eng->rx); rX.m[6]=sinf(eng->rx); rX.m[9]=-sinf(eng->rx); rX.m[10]=cosf(eng->rx);
    mat4_mul(&pv,&rX,&rY); mat4_mul(&pv,&pv,&vi); mat4_mul(&pv,&pr,&pv);

    for(int ix=0;ix<16;ix++) for(int iz=0;iz<16;iz++) for(int iy=0;iy<3;iy++) {
        if(WORLD[ix][iz][iy]==B_EMPTY) continue;
        mat4_id(&mo); mo.m[12]=(float)ix; mo.m[13]=(float)iy; mo.m[14]=(float)iz;
        mat4_mul(&mv,&pv,&mo);
        switch(WORLD[ix][iz][iy]) {
            case B_FLOOR: draw_box(eng,&mv,0.15f,0.15f,0.15f,1,0); break; // Тёмный пол
            case B_WALL:  draw_box(eng,&mv,0.2f,0.2f,0.22f,1,0); break;   // Серая стена
            case B_DOOR:  draw_box(eng,&mv,0.3f,0.2f,0.1f,1,0); break;    // Коричневая дверь
            case B_FUSE:  draw_box(eng,&mv,0.9f,0.1f,0.1f,1,0); break;    // Красный предохранитель
            case B_PANEL: draw_box(eng,&mv,0.1f,0.1f,0.9f,1,0); break;    // Синий щиток
            case B_EXIT:  draw_box(eng,&mv,0.1f,0.9f,0.1f,1,0); break;    // Зелёный выход
        }
    }

    // UI
    glDisable(GL_DEPTH_TEST); mat4 or; mat4_id(&or); or.m[0]=2.0f/w; or.m[5]=-2.0f/h; or.m[12]=-1; or.m[13]=1;

    // Джойстик
    if(eng->tj) {
        mat4_id(&mo); mo.m[12]=eng->jsx; mo.m[13]=eng->jsy; mo.m[0]=160; mo.m[5]=160;
        mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,1,1,1,0.2f,2);
        mat4_id(&mo); mo.m[12]=eng->jcx; mo.m[13]=eng->jcy; mo.m[0]=65; mo.m[5]=65;
        mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,0,0,0,0.6f,2);
    }

    // Кнопка взаимодействия (правый нижний угол)
    mat4_id(&mo); mo.m[12]=w-120; mo.m[13]=h-120; mo.m[0]=160; mo.m[5]=160;
    mat4_mul(&mv,&or,&mo); draw_box(eng,&mv,0.8f,0.8f,0.1f,0.5f,2);

    // FPS
    char fps_str[8]; sprintf(fps_str,"%d",eng->fps);
    draw_text(eng,fps_str,w-80,30,&or,1,1,0);

    // Предохранители HUD
    char fuse_str[8]; sprintf(fuse_str,"%d",eng->gs.fuses_found);
    draw_text(eng,"3",30,30,&or,0.5f,0.5f,0.5f);
    draw_text(eng,fuse_str,30,30,&or,0.9f,0.1f,0.1f);

    // Сообщение в центре
    if(eng->gs.msg_timer>0) draw_text(eng,eng->gs.msg,w/2-100,h/2,&or,1,1,1);

    glEnable(GL_DEPTH_TEST); eglSwapBuffers(eng->disp,eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng=(struct engine*)app->userData;
    if(AInputEvent_getType(ev)!=AINPUT_EVENT_TYPE_MOTION) return 0;
    int a=AMotionEvent_getAction(ev)&AMOTION_EVENT_ACTION_MASK;
    int i=(AMotionEvent_getAction(ev)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float x=AMotionEvent_getX(ev,i),y=AMotionEvent_getY(ev,i),w=ANativeWindow_getWidth(app->window),h=ANativeWindow_getHeight(app->window);
    if(a==AMOTION_EVENT_ACTION_DOWN||a==AMOTION_EVENT_ACTION_POINTER_DOWN) {
        // Кнопка взаимодействия (правый нижний угол)
        if(x>w-200&&y>h-200) { interact(eng); return 1; }
        if(x<w/2) { eng->tj=1; eng->jsx=x; eng->jsy=y; eng->jcx=x; eng->jcy=y; }
        else { eng->tl=1; eng->lsx=x; eng->lsy=y; }
    } else if(a==AMOTION_EVENT_ACTION_MOVE) {
        for(int p=0;p<AMotionEvent_getPointerCount(ev);p++) {
            float px=AMotionEvent_getX(ev,p),py=AMotionEvent_getY(ev,p);
            if(px<w/2) { float dx=px-eng->jsx,dy=py-eng->jsy,d=sqrtf(dx*dx+dy*dy); if(d>70){dx*=70/d;dy*=70/d;} eng->jcx=eng->jsx+dx; eng->jcy=eng->jsy+dy; }
            else { eng->ry+=(px-eng->lsx)*0.005f; eng->rx=fmaxf(-1.4f,fminf(1.4f,eng->rx+(py-eng->lsy)*0.005f)); eng->lsx=px; eng->lsy=py; }
        }
    } else if(a==AMOTION_EVENT_ACTION_UP||a==AMOTION_EVENT_ACTION_POINTER_UP) {
        if(x<w/2) eng->tj=0; else eng->tl=0;
    }
    return 1;
}

static void handle_cmd(struct android_app* app, int32_t cmd) {
    struct engine* eng=(struct engine*)app->userData;
    if(cmd==APP_CMD_INIT_WINDOW) {
        eng->disp=eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(eng->disp,0,0);
        EGLConfig cfg; EGLint n; eglChooseConfig(eng->disp,(EGLint[]){EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_BLUE_SIZE,8,EGL_DEPTH_SIZE,16,EGL_NONE},&cfg,1,&n);
        eng->surf=eglCreateWindowSurface(eng->disp,cfg,eng->app->window,NULL);
        eng->ctx=eglCreateContext(eng->disp,cfg,NULL,(EGLint[]){EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE});
        eglMakeCurrent(eng->disp,eng->surf,eng->surf,eng->ctx);
        GLuint vs=glCreateShader(GL_VERTEX_SHADER); glShaderSource(vs,1,&VS,0); glCompileShader(vs);
        GLuint fs=glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(fs,1,&FS,0); glCompileShader(fs);
        eng->prog=glCreateProgram(); glAttachShader(eng->prog,vs); glAttachShader(eng->prog,fs); glLinkProgram(eng->prog);
        eng->mvp_loc=glGetUniformLocation(eng->prog,"m"); eng->col_loc=glGetUniformLocation(eng->prog,"c"); eng->mode_loc=glGetUniformLocation(eng->prog,"mode");
        glEnable(GL_DEPTH_TEST);
        build_world();
        eng->x=2.0f; eng->z=2.0f;
        sprintf(eng->gs.msg,"Найди 3 предохранителя!");
        eng->gs.msg_timer=200;
    } else if(cmd==APP_CMD_TERM_WINDOW) eng->disp=NULL;
}

void android_main(struct android_app* state) {
    struct engine eng={0}; state->userData=&eng; state->onAppCmd=handle_cmd; state->onInputEvent=handle_input; eng.app=state;
    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(eng.disp?0:-1,0,&ev,(void**)&src)>=0) { if(src) src->process(state,src); if(state->destroyRequested) return; }
        if(eng.disp) draw(&eng);
    }
}

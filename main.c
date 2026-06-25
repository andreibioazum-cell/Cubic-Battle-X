#include <android_native_app_glue.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <time.h>
#include <stdio.h>
#include "game_logic.h"
#include "shaders.h"
#include "primitives.h"
#include "font.h"

struct engine {
    struct android_app* app; EGLDisplay disp; EGLSurface surf; EGLContext ctx;
    GLuint prog; GLint mvp_loc, col_loc, mode_loc; Game game;
};

void draw_box(struct engine* eng, mat4* mvp, float r, float g, float b, float a, int mode) {
    glUniformMatrix4fv(eng->mvp_loc, 1, 0, mvp->m);
    glUniform4f(eng->col_loc, r, g, b, a);
    glUniform1i(eng->mode_loc, mode);
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 6*4, CUBE); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, 0, 6*4, &CUBE[3]); glEnableVertexAttribArray(1);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, IND);
}

void draw_text(struct engine* eng, int val, float x, float y, mat4* ortho) {
    char buf[16]; int len = sprintf(buf, "%d", val); float s = 8.0f;
    for(int i=0; i<len; i++) {
        int d = buf[i] - '0';
        for(int py=0; py<8; py++) for(int px=0; px<8; px++) {
            if((FONT_8x8[d][py]>>(7-px))&1) {
                mat4 mo, mv; mat4_id(&mo); mo.m[12]=x+i*70+px*s; mo.m[13]=y+py*s; mo.m[0]=s; mo.m[5]=s;
                mat4_mul(&mv, ortho, &mo); draw_box(eng, &mv, 1, 1, 0, 1, 2);
            }
        }
    }
}

static void draw(struct engine* eng) {
    if (!eng->disp) return;
    int w=ANativeWindow_getWidth(eng->app->window), h=ANativeWindow_getHeight(eng->app->window);
    Game* g = &eng->game;
    
    // FPS
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    long t = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    static int fc = 0; fc++; if(t-g->last_t>=1000){ g->fps=fc; fc=0; g->last_t=t; }

    if(g->tj) {
        float dx=(g->jcx-g->jsx)/100.0f, dy=(g->jcy-g->jsy)/100.0f;
        float s=sinf(g->ry), c=cosf(g->ry);
        g->x+=(dx*c-dy*s)*0.1f; g->z+=(dx*s+dy*c)*0.1f;
    }
    if(g->r_act) {
        g->r_x+=g->r_dx; g->r_y+=g->r_dy; g->r_z+=g->r_dz;
        if(sqrtf(pow(g->r_x-g->x,2)+pow(g->r_z-g->z,2)) > 15.0f) g->r_act=0;
    }

    glViewport(0,0,w,h); glClearColor(0.05, 0.05, 0.08, 1.0); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glUseProgram(eng->prog); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mat4 pr, vi, rY, rX, pv, mo, mv;
    mat4_perspective(&pr, 1.0f, (float)w/h, 0.1f, 100.0f);
    mat4_id(&vi); vi.m[12]=-g->x; vi.m[13]=-1.6f; vi.m[14]=-g->z;
    mat4_id(&rY); rY.m[0]=cosf(g->ry); rY.m[2]=-sinf(g->ry); rY.m[8]=sinf(g->ry); rY.m[10]=cosf(g->ry);
    mat4_id(&rX); rX.m[5]=cosf(g->rx); rX.m[6]=sinf(g->rx); rX.m[9]=-sinf(g->rx); rX.m[10]=cosf(g->rx);
    mat4_mul(&pv, &rX, &rY); mat4_mul(&pv, &pv, &vi); mat4_mul(&pv, &pr, &pv);

    for(int i=0;i<16;i++) for(int j=0;j<16;j++) {
        if(WORLD_MAP[i][j]==0) continue;
        mat4_id(&mo); mo.m[12]=i*2.0f; mo.m[14]=j*2.0f; mo.m[0]=2; mo.m[10]=2; mo.m[5]=4;
        mat4_mul(&mv, &pv, &mo); draw_box(eng, &mv, 0.3, 0.3, 0.35, 1, 0);
    }
    mat4_id(&mo); mo.m[0]=100; mo.m[10]=100; mo.m[5]=0.1;
    mat4_mul(&mv, &pv, &mo); draw_box(eng, &mv, 0.15, 0.15, 0.15, 1, 0);

    // Руки
    glDisable(GL_DEPTH_TEST);
    mat4_id(&mo); mo.m[12]=0.6; mo.m[13]=-0.6; mo.m[14]=-1.2; mo.m[0]=0.4; mo.m[5]=0.4; mo.m[10]=0.4;
    mat4_mul(&mv, &pr, &mo); draw_box(eng, &mv, 0.1, 0.1, 0.1, 1, 0);
    mo.m[12]=-0.6; mat4_mul(&mv, &pr, &mo); draw_box(eng, &mv, 0.1, 0.1, 0.1, 1, 0);
    if(g->r_act) {
        mat4_id(&mo); mo.m[12]=g->r_x; mo.m[13]=g->r_y; mo.m[14]=g->r_z; mo.m[0]=0.4; mo.m[5]=0.4; mo.m[10]=0.4;
        mat4_mul(&mv, &pv, &mo); draw_box(eng, &mv, 0, 0.4, 1, 1, 0);
    }

    // UI
    mat4 or; mat4_id(&or); or.m[0]=2.0f/w; or.m[5]=-2.0f/h; or.m[12]=-1; or.m[13]=1;
    if(g->tj) {
        mat4_id(&mo); mo.m[12]=g->jsx; mo.m[13]=g->jsy; mo.m[0]=160; mo.m[5]=160;
        mat4_mul(&mv, &or, &mo); draw_box(eng, &mv, 0, 0, 0, 0.4, 2);
        float dx=g->jcx-g->jsx, dy=g->jcy-g->jsy, d=sqrtf(dx*dx+dy*dy); if(d>70){dx*=70/d;dy*=70/d;}
        mat4_id(&mo); mo.m[12]=g->jsx+dx; mo.m[13]=g->jsy+dy; mo.m[0]=60; mo.m[5]=60;
        mat4_mul(&mv, &or, &mo); draw_box(eng, &mv, 1, 1, 1, 1, 2);
    }
    mat4_id(&mo); mo.m[12]=w-150; mo.m[13]=h-150; mo.m[0]=120; mo.m[5]=120;
    mat4_mul(&mv, &or, &mo); draw_box(eng, &mv, 0, 0.5, 1, 0.5, 2);
    draw_text(eng, g->fps, w-140, 50, &or);
    glEnable(GL_DEPTH_TEST); eglSwapBuffers(eng->disp, eng->surf);
}

static int32_t handle_input(struct android_app* app, AInputEvent* ev) {
    struct engine* eng=(struct engine*)app->userData; Game* g=&eng->game;
    if (AInputEvent_getType(ev) != AINPUT_EVENT_TYPE_MOTION) return 0;
    int a=AMotionEvent_getAction(ev)&AMOTION_EVENT_ACTION_MASK;
    int i=(AMotionEvent_getAction(ev)&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    float x=AMotionEvent_getX(ev,i), y=AMotionEvent_getY(ev,i), w=ANativeWindow_getWidth(app->window), h=ANativeWindow_getHeight(app->window);
    if(a==AMOTION_EVENT_ACTION_DOWN || a==AMOTION_EVENT_ACTION_POINTER_DOWN) {
        if(x>w-250 && y>h-250) { g->r_act=1; g->r_x=g->x; g->r_y=1.6; g->r_z=g->z; g->r_dx=sinf(g->ry)*0.6f; g->r_dy=sinf(g->rx)*0.6f; g->r_dz=cosf(g->ry)*0.6f; }
        else if(x<w/2) { g->tj=1; g->jsx=x; g->jsy=y; g->jcx=x; g->jcy=y; }
        else { g->tl=1; g->lsx=x; g->lsy=y; }
    } else if(a==AMOTION_EVENT_ACTION_MOVE) {
        for(int p=0;p<AMotionEvent_getPointerCount(ev);p++){
            float px=AMotionEvent_getX(ev,p), py=AMotionEvent_getY(ev,p);
            if(px<w/2) { g->jcx=px; g->jcy=py; }
            else { g->ry+=(px-g->lsx)*0.005f; g->rx=fmaxf(-1.4f,fminf(1.4f,g->rx+(py-g->lsy)*0.005f)); g->lsx=px; g->lsy=py; }
        }
    } else if(a==AMOTION_EVENT_ACTION_UP || a==AMOTION_EVENT_ACTION_POINTER_UP) { if(x<w/2) g->tj=0; else g->tl=0; }
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
    } else if(cmd==APP_CMD_TERM_WINDOW) eng->disp=NULL;
}

void android_main(struct android_app* state) {
    struct engine eng={0}; state->userData=&eng; state->onAppCmd=handle_cmd; state->onInputEvent=handle_input; eng.app=state;
    while(1) {
        int ev; struct android_poll_source* src;
        while(ALooper_pollOnce(eng.disp?0:-1, 0, &ev, (void**)&src)>=0) { if(src) src->process(state,src); if(state->destroyRequested) return; }
        if(eng.disp) draw(&eng);
    }
}

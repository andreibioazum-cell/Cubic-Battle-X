#define SOKOL_GLES2
#define SOKOL_ANDROID
#define SOKOL_IMPL
#include "https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h"
#include "https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h"
#include "https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h"
#include <math.h>

// Вершинный шейдер
const char* vs_src = 
    "uniform mat4 mvp; attribute vec4 position; attribute vec4 color; varying vec4 v_color; "
    "void main() { gl_Position = mvp * position; v_color = color; }";

// Фрагментный шейдер
const char* fs_src = 
    "precision mediump float; varying vec4 v_color; void main() { gl_FragColor = v_color; }";

static struct {
    sg_pipeline pip;
    sg_bindings bind;
    float rx, ry;
} state;

// Инициализация игры
void init(void) {
    sg_setup(&(sg_desc){ .context = sokol_helper_backend_desc() });

    // Куб: позиции и цвета
    float vertices[] = {
        -0.5f,-0.5f,-0.5f, 1.0f,0.0f,0.0f,1.0f,  0.5f,-0.5f,-0.5f, 1.0f,0.0f,0.0f,1.0f,
         0.5f, 0.5f,-0.5f, 1.0f,0.0f,0.0f,1.0f, -0.5f, 0.5f,-0.5f, 1.0f,0.0f,0.0f,1.0f,
        -0.5f,-0.5f, 0.5f, 0.0f,1.0f,0.0f,1.0f,  0.5f,-0.5f, 0.5f, 0.0f,1.0f,0.0f,1.0f,
         0.5f, 0.5f, 0.5f, 0.0f,1.0f,0.0f,1.0f, -0.5f, 0.5f, 0.5f, 0.0f,1.0f,0.0f,1.0f
    };
    state.bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){ .data = SG_RANGE(vertices) });

    uint16_t indices[] = { 0,1,2, 0,2,3, 6,5,4, 7,6,4, 1,0,4, 5,1,4, 2,1,5, 2,5,6, 3,2,6, 3,6,7, 0,3,7, 0,7,4 };
    state.bind.index_buffer = sg_make_buffer(&(sg_buffer_desc){ .type = SG_BUFFERTYPE_INDEXBUFFER, .data = SG_RANGE(indices) });

    state.pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(&(sg_shader_desc){
            .vs.source = vs_src, .fs.source = fs_src,
            .vs.uniform_blocks[0].size = 64
        }),
        .layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3,
        .layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4,
        .index_type = SG_INDEXTYPE_UINT16,
        .depth = { .compare = SG_COMPAREFUNC_LESS_EQUAL, .write_enabled = true }
    });
}

// Кадр анимации
void frame(void) {
    state.rx += 1.0f; state.ry += 2.0f;
    sg_pass_action pass_action = { .colors[0] = { .action = SG_ACTION_CLEAR, .value = {0.1f, 0.1f, 0.3f, 1.0f} } };
    sg_begin_default_pass(&pass_action, sapp_width(), sapp_height());
    sg_apply_pipeline(state.pip);
    sg_apply_bindings(&state.bind);
    // (Тут должна быть матрица MVP, для краткости опустим её расчет)
    sg_draw(0, 36, 1);
    sg_end_pass();
    sg_commit();
}

sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc){ .init_cb = init, .frame_cb = frame, .width = 800, .height = 600, .window_title = "Cubic Battle" };
}

#include <math.h>
#include <stdbool.h>
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"

#define MAP_W 8
#define MAP_H 8

#define FOV 1.047f //60degrees in rad

int map[MAP_H][MAP_W] = {
    {1,2,1,2,1,2,1,2},
    {2,0,0,0,0,0,0,1},
    {2,0,0,0,0,3,0,2},
    {2,0,0,0,0,0,0,1},
    {2,0,0,0,0,0,0,2},
    {2,0,3,0,0,0,0,1},
    {2,0,0,0,3,0,0,2},
    {2,1,2,1,2,1,2,1},
};

float p_x = 4.5f;
float p_y = 4.5f;
float p_angle = 1.57f; //in rad

static sg_buffer pos_buf;
static sg_buffer col_buf;
static sg_pipeline pip;
static sg_bindings r_bind;
static sg_image offscreen_img;
static sg_view offscreen_view;
static sg_view offscreen_texture_view;
static sg_pipeline display_pip;
static sg_bindings display_bind;
static sg_sampler display_sampler;

int key_w = 0;
int key_s = 0;
int key_a = 0;
int key_d = 0;
int key_q = 0;
int key_e = 0;


typedef struct {
    float distance;
    int wall_t;
    int side;
} RayHit;


const char *vs_source =
"#version 330 core\n"
"layout(location=0) in vec2 position;\n"
"layout(location=1) in vec3 color;\n"
"out vec3 v_color;\n"
"void main() {\n"
"   gl_Position = vec4(position, 0.0, 1.0);\n" 
"   v_color = color;\n"
"}\n";

const char *fs_source =
"#version 330 core\n"
"in vec3 v_color;\n"
"out vec4 frag_color;\n"
"void main() {\n"
"   frag_color = vec4(v_color, 1.0);\n"
"}\n";

const char *display_vs_source =
"#version 330 core\n"
"layout(location=0) in vec2 position;\n"
"layout(location=1) in vec2 texcoord;\n"
"out vec2 uv;\n"
"void main(){\n"
"   gl_Position = vec4(position, 0.0, 1.0);\n"
"   uv = texcoord;\n"
"}\n";

const char *display_fs_source =
"#version 330 core\n"
"uniform sampler2D tex;\n"
"in vec2 uv;\n"
"out vec4 frag_color;\n"
"void main(){\n"
"   frag_color = texture(tex,uv);\n"
"}\n";

void event(const sapp_event* e) {
        if (e->type == SAPP_EVENTTYPE_KEY_DOWN) {
        if (e->key_code == SAPP_KEYCODE_W) key_w = 1;
        if (e->key_code == SAPP_KEYCODE_S) key_s = 1;
        if (e->key_code == SAPP_KEYCODE_A) key_a = 1;
        if (e->key_code == SAPP_KEYCODE_D) key_d = 1;
        if (e->key_code == SAPP_KEYCODE_Q) key_q = 1;
        if (e->key_code == SAPP_KEYCODE_E) key_e = 1;
    }
    
    if (e->type == SAPP_EVENTTYPE_KEY_UP) {
        if (e->key_code == SAPP_KEYCODE_W) key_w = 0;
        if (e->key_code == SAPP_KEYCODE_S) key_s = 0;
        if (e->key_code == SAPP_KEYCODE_A) key_a = 0;
        if (e->key_code == SAPP_KEYCODE_D) key_d = 0;
        if (e->key_code == SAPP_KEYCODE_Q) key_q = 0;
        if (e->key_code == SAPP_KEYCODE_E) key_e = 0;
    }
}





RayHit cast_ray(float ray_angle) {
    float ray_x = p_x;
    float ray_y = p_y;

    float ray_dir_x = cos(ray_angle);
    float ray_dir_y = sin(ray_angle);
    float step_size = 0.05f;
    int side = 0;
    for (int i = 0; i < 200; i++) {
        ray_x += ray_dir_x * step_size;
        ray_y += ray_dir_y * step_size;
        int map_x = (int)ray_x;
        int map_y = (int)ray_y;
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
            return (RayHit){100.0f, 0, 0};
        }
        if (map[(int)ray_y][(int)ray_x] != 0) {
            float dx = ray_x - p_x;
            float dy = ray_y - p_y;
            float dist = sqrtf(dx*dx + dy*dy);
            float fr_x = ray_x - map_x;
            float fr_y = ray_y - map_y;
            if (fr_x  < 0.1 || fr_x > 0.9) {
                side = 1;
            } else {
                side = 0;
            }
            int wall_t = map[map_y][map_x];
            return (RayHit){dist, wall_t, side};
        }
    }
    return (RayHit){100.0f, 0, 0};
}


void init(void) {

    sg_setup(&(sg_desc) {
            .environment = sglue_environment(),
            .logger.func = slog_func,
            });

     offscreen_img = sg_make_image(&(sg_image_desc){
            .usage.color_attachment = true,
            .width = 400,
            .height = 300,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            });
    offscreen_view = sg_make_view(&(sg_view_desc){
            .color_attachment.image = offscreen_img,
            });
    offscreen_texture_view = sg_make_view(&(sg_view_desc){
            .texture.image = offscreen_img
            });

    float quad_vertices[] = {
         -1.0f, -1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 1.0f,
         -1.0f, 1.0f,   0.0f, 0.0f,
         1.0f, 1.0f,   1.0f, 0.0f,
    };
    display_bind.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(quad_vertices)
            });
    display_sampler = sg_make_sampler(&(sg_sampler_desc){ //nearest pixel no average
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST
            });
    display_bind.samplers[0] = display_sampler;
    display_bind.views[0] = offscreen_texture_view;



    sg_shader display_shd = sg_make_shader(&(sg_shader_desc){
            .vertex_func = { .source = display_vs_source },
            .fragment_func = { .source = display_fs_source },
            .views[0] = {
                .texture = {
                    .stage = SG_SHADERSTAGE_FRAGMENT,
                    .image_type = SG_IMAGETYPE_2D,
                    .sample_type = SG_IMAGESAMPLETYPE_FLOAT,
                }
            },
            .samplers[0] = {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .sampler_type = SG_SAMPLERTYPE_FILTERING,
            },
            .texture_sampler_pairs[0] = {
                .stage = SG_SHADERSTAGE_FRAGMENT,
                .view_slot = 0,
                .sampler_slot = 0,
                .glsl_name = "tex",
            }
    });

    display_pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = display_shd,
            .layout = {
                .attrs[0] = { .format = SG_VERTEXFORMAT_FLOAT2 },
                .attrs[1] = { .format = SG_VERTEXFORMAT_FLOAT2 },
            },
            .primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP,
    });
   
    float vertex_pos[] = {
        -0.0025f, 1.0f,
        -0.0025f, -1.0f,
        0.0025f, -1.0f,

        -0.0025f, 1.0f,
        0.0025f, -1.0f,
        0.0025f, 1.0f,
    };
    float vertex_col[] = {
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
    };
    pos_buf = sg_make_buffer(&(sg_buffer_desc){
            .usage = {
                .vertex_buffer = true,
                .dynamic_update = true,
            },
            .size = sizeof(float) * 2 * 6 * 400,
            });

    col_buf = sg_make_buffer(&(sg_buffer_desc){
            .usage = {
                .vertex_buffer = true,
                .dynamic_update = true,
            },
            .size = sizeof(float) * 3 * 6 * 400,
            });

    sg_shader shad = sg_make_shader(&(sg_shader_desc){
            .vertex_func = { .source = vs_source},
            .fragment_func = {.source = fs_source}
            });

    pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = shad,
            .layout = {
            .attrs[0] = {
                .format = SG_VERTEXFORMAT_FLOAT2,
                .buffer_index = 0
            },
            .attrs[1] = {
                .format = SG_VERTEXFORMAT_FLOAT3,
                .buffer_index = 1
            }
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_NONE,
            .write_enabled = false,
            .compare = SG_COMPAREFUNC_ALWAYS,
        },
    });
    r_bind.vertex_buffers[0] = pos_buf;
    r_bind.vertex_buffers[1] = col_buf;

}

void frame(void) {

    if (key_w) {
        float new_x = p_x + cosf(p_angle) * 0.05f;
        float new_y = p_y + sinf(p_angle) * 0.05f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }

    if (key_s) {
        float new_x = p_x - cosf(p_angle) * 0.05f;
        float new_y = p_y - sinf(p_angle) * 0.05f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }
    

    if (key_a) {
        float new_x = p_x + cosf(p_angle - 1.57f) * 0.04f;
        float new_y = p_y + sinf(p_angle - 1.57f) * 0.05f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }

    if (key_d) {
        float new_x = p_x + cosf(p_angle + 1.57f) * 0.04f;
        float new_y = p_y + sinf(p_angle + 1.57f) * 0.04f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }

    if (key_q) p_angle -= 0.03f;
    if (key_e) p_angle += 0.03f;

    //first pass (offscren)
    sg_begin_pass(&(sg_pass){
            .action = {
                .colors[0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .clear_value = {0.05f, 0.1f, 0.65f, 1.0f}
                }
            },
            .attachments = {
            .colors[0] = offscreen_view,
            }
    });
    sg_apply_pipeline(pip);
    sg_apply_bindings(&r_bind);

    float positions[400*6*2];
    float colors[400*6*3];
    int pos_idx = 0;
    int col_idx = 0;

    for (int i = 0; i < 400; i++) {
        float screen_x = (2.0f * i/ 400.0f) - 1.0f;
        float ray_angle = p_angle + screen_x * (FOV/2.0f);

        RayHit hit = cast_ray(ray_angle);
        float distance  = hit.distance;
        float p_distance = distance * cosf(ray_angle - p_angle);

        float wall_h = 1.0f / p_distance;
        if (wall_h > 1.0f) wall_h = 1.0f; //clamparooo

        float x_l = (2.0f * i /400.0f) - 1.0f;
        float x_r = (2.0f * (i + 1)/ 400.0f) - 1.0f;
        
        positions[pos_idx++] = x_l; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_l; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -wall_h;

        positions[pos_idx++] = x_l; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = wall_h;

        //test colors different walls and some side shades
        float r,g,b;
        if (hit.wall_t == 1) {
            r = 0.1f; g = 0.4f; b = 0.1f;
        } else if (hit.wall_t == 2) {
            r = 0.3f; g = 0.2f; b = 0.1f;
        } else {
            r = 0.1f; g = 0.6f; b = 0.4f;
        }
        if (hit.side == 0) {
            r *= 1.0f;
            g *= 1.0f;
            b *= 1.0f;
        } else {
            r *= 0.75f;
            g *= 0.75f;
            b *= 0.75f;
        }
        float shade = 1.0f - (hit.distance / 10.0f);
        if (shade < 0.2f) shade = 0.2f;
        r *= shade;
        g *= shade;
        b *= shade;
        

        for (int v = 0; v < 6; v++) {
            colors[col_idx++] = r;
            colors[col_idx++] = g;
            colors[col_idx++] = b;
        }

    }
    sg_update_buffer(pos_buf, &(sg_range){
        .ptr = positions,
        .size = pos_idx * sizeof(float)
    });
    
    sg_update_buffer(col_buf, &(sg_range){
        .ptr = colors,
        .size = col_idx * sizeof(float)
    });
    sg_draw(0, pos_idx/2, 1);
    sg_end_pass();

    //pass 2
    sg_begin_pass(&(sg_pass){
            .action = {
                .colors[0] = {
                    .load_action = SG_LOADACTION_CLEAR,
                    .clear_value = {0.0f, 0.0f, 0.0f, 1.0f}
                }
            },
            .swapchain = sglue_swapchain()
    });


    sg_apply_pipeline(display_pip);
    sg_apply_bindings(&display_bind);
    sg_draw(0, 4, 1);
    
    sg_end_pass();
    sg_commit();
}

void cleanup(void) {
    sg_shutdown();
}


sapp_desc sokol_main(int argc, char* argv[]) {
    return (sapp_desc) {
        .init_cb = init,
        .frame_cb = frame,
        .cleanup_cb = cleanup,
        .event_cb = event,
        .width = 800,
        .height = 600,
        .high_dpi = false,
        .window_title = "test 2.5d"
    };
}

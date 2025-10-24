#include <math.h>
#include <stdbool.h>
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"

#define MAP_W 16
#define MAP_H 16

#define FOV 1.047f //60degrees in rad

#define RENDER_HEIGHT 300

int map[MAP_H][MAP_W] = {
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,1,1,1,1,1,1,0,1,1,1,1,1,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,1,0,2},
    {2,0,1,1,1,1,0,1,1,1,1,1,0,1,0,2},
    {2,0,1,0,0,0,0,0,0,0,0,1,0,0,0,2},
    {2,0,1,0,1,1,1,1,1,1,0,1,1,1,0,2},
    {2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,2},
    {2,0,1,1,1,1,0,1,1,1,1,1,0,1,0,2},
    {2,0,0,0,0,1,0,0,0,0,0,1,0,1,0,2},
    {2,0,1,1,0,1,1,1,1,1,0,1,0,1,0,2},
    {2,0,1,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,1,0,1,1,1,1,1,1,1,1,1,1,0,2},
    {2,0,0,0,0,0,0,3,0,0,0,0,0,3,0,2},
    {2,0,1,1,1,1,1,1,1,1,1,1,1,1,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2}
};

float p_x = 1.5f;
float p_y = 1.5f;
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


RayHit cast_ray_dda(float ray_angle) {
    float ray_dir_x = cos(ray_angle);
    float ray_dir_y = sin(ray_angle);

    int map_x = (int)p_x;
    int map_y = (int)p_y;

    int step_x = (ray_dir_x >= 0) ? 1 : -1; 
    int step_y = (ray_dir_y >= 0) ? 1 : -1; 

    float next_boundary_x = (step_x > 0) ? (map_x + 1) : map_x;
    float distance_to_b_x = (step_x > 0) ? (next_boundary_x - p_x) : (p_x - next_boundary_x);
    float next_boundary_y = (step_y > 0) ? (map_y + 1) : map_y;
    float distance_to_b_y = (step_y > 0) ? (next_boundary_y - p_y) : (p_y - next_boundary_y);

    float delta_x = distance_to_b_x / fabs(ray_dir_x);
    float delta_y = distance_to_b_y / fabs(ray_dir_y);
    float delta_d_x = 1.0f / fabs(ray_dir_x);
    float delta_d_y = 1.0f / fabs(ray_dir_y);

    int side = 0;

    for (int i = 0; i < MAP_H + MAP_W; i++) {
        if (delta_x < delta_y) {
            map_x += step_x;
            delta_x += delta_d_x;
            side = 1;
        } else {
            map_y += step_y;
            delta_y += delta_d_y;
            side = 0;
        }
        if (map_x < 0 || map_x >= MAP_W || map_y < 0 || map_y >= MAP_H) {
            return (RayHit){100.0f, 0, 0};
        } 
        if (map[map_y][map_x] != 0) {
            float distance = (side == 1) ?  delta_x - delta_d_x : delta_y - delta_d_y;
            int wall_type = map[map_y][map_x];
            return (RayHit){distance, wall_type, side};
        }
    }
    return (RayHit){100.0f, 0, 0};
}





RayHit cast_ray(float ray_angle) {
    float ray_x = p_x;
    float ray_y = p_y;


    float ray_dir_x = cos(ray_angle);
    float ray_dir_y = sin(ray_angle);
    float step_size = 0.05f;
    int side = 0;
    for (int i = 0; i < 500; i++) {
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
            .width = 480,
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
            .size = sizeof(float) * 2 * 18 * 480,
            });

    col_buf = sg_make_buffer(&(sg_buffer_desc){
            .usage = {
                .vertex_buffer = true,
                .dynamic_update = true,
            },
            .size = sizeof(float) * 3 * 18 * 480,
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
        float new_x = p_x + cosf(p_angle) * 0.03f;
        float new_y = p_y + sinf(p_angle) * 0.03f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }

    if (key_s) {
        float new_x = p_x - cosf(p_angle) * 0.03f;
        float new_y = p_y - sinf(p_angle) * 0.03f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }


    if (key_a) {
        float new_x = p_x + cosf(p_angle - 1.57f) * 0.02f;
        float new_y = p_y + sinf(p_angle - 1.57f) * 0.03f;
        if (map[(int)new_y][(int)new_x] == 0) {
            p_x = new_x;
            p_y = new_y;
        }
    }

    if (key_d) {
        float new_x = p_x + cosf(p_angle + 1.57f) * 0.02f;
        float new_y = p_y + sinf(p_angle + 1.57f) * 0.02f;
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

    float positions[480*18*2];
    float colors[480*18*3];
    int pos_idx = 0;
    int col_idx = 0;

    for (int i = 0; i < 480; i++) {
        float screen_x = (2.0f * i/ 480.0f) - 1.0f;
        float ray_angle = p_angle + screen_x * (FOV/2.0f);

        RayHit hit = cast_ray(ray_angle);
        float distance  = hit.distance;
        float p_distance = distance * cosf(ray_angle - p_angle);

        float wall_h = 1.0f / p_distance;
        if (wall_h > 1.0f) wall_h = 1.0f; //clamparooo

        float x_l = (2.0f * i /480.0f) - 1.0f;
        float x_r = (2.0f * (i + 1)/ 480.0f) - 1.0f;

        float test_fog_r = 0.3f;
        float test_fog_g = 0.3f;
        float test_fog_b = 0.3f;
        float fog_max_dist = 8.0f;


        //ceiling 
        positions[pos_idx++] = x_l; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_l; positions[pos_idx++] = -1.0f;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -1.0f;
        positions[pos_idx++] = x_l; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -1.0f;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -wall_h;

        // Calculate projection distance and shades
        float pr_dist = (RENDER_HEIGHT / 2.0f) / tanf(FOV / 2.0f); 

        // Ceiling shading
        float y_t = -wall_h;
        float y_t_pix = y_t * (RENDER_HEIGHT / 2.0f);
        float dist_from_h_tp = fabs(y_t_pix);
        float ceil_t_d = (0.5f * pr_dist) / dist_from_h_tp;
        float tp_shade = 1.0f - (ceil_t_d / 3.0f);
        if (tp_shade < 0.1f) tp_shade = 0.1f;

        float y_b = -1.0f;
        float y_b_pix = y_b * (RENDER_HEIGHT / 2.0f);
        float dist_from_h_btm = fabs(y_b_pix);
        float ceil_b_d = (0.5f * pr_dist) / dist_from_h_btm;
        float btm_shade = 1.0f - (ceil_b_d / 3.0f);
        if (btm_shade < 0.1f) btm_shade = 0.1f;

        // Floor shading
        float y_floor_top = 1.0f;
        float y_floor_top_pix = y_floor_top * (RENDER_HEIGHT / 2.0f);
        float dist_from_h_floor_top = fabsf(y_floor_top_pix);
        float floor_top_d = (0.5f * pr_dist) / dist_from_h_floor_top;
        float floor_top_shade = 1.0f - (floor_top_d / 3.0f);
        if (floor_top_shade < 0.1f) floor_top_shade = 0.1f;

        float y_floor_btm = wall_h;
        float y_floor_btm_pix = y_floor_btm * (RENDER_HEIGHT / 2.0f);
        float dist_from_h_floor_btm = fabsf(y_floor_btm_pix);
        float floor_btm_d = (0.5f * pr_dist) / dist_from_h_floor_btm;
        float floor_btm_shade = 1.0f - (floor_btm_d / 3.0f);
        if (floor_btm_shade < 0.1f) floor_btm_shade = 0.1f;

        // Ceiling colors (deliberate: using tp_shade for "reflection" effect)
        for (int v = 0; v < 6; v++) {
            colors[col_idx++] = 0.2f * tp_shade;
            colors[col_idx++] = 0.2f * tp_shade;
            colors[col_idx++] = 0.2f * tp_shade;
        }

        //floor (renders at bottom of screen)
        positions[pos_idx++] = x_l; positions[pos_idx++] = 1.0f;
        positions[pos_idx++] = x_l; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_l; positions[pos_idx++] = 1.0f;
        positions[pos_idx++] = x_r; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = 1.0f;

        // Floor colors (match vertex order: close, far, far, close, far, close)
        // Vertex 1: y=1.0 (close)
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        // Vertex 2: y=wall_h (far)
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        // Vertex 3: y=wall_h (far)
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        // Vertex 4: y=1.0 (close)
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        // Vertex 5: y=wall_h (far)
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        colors[col_idx++] = 0.2f * floor_btm_shade;
        // Vertex 6: y=1.0 (close)
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        colors[col_idx++] = 0.2f * floor_top_shade;
        //walls
        positions[pos_idx++] = x_l; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_l; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_l; positions[pos_idx++] = wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = -wall_h;
        positions[pos_idx++] = x_r; positions[pos_idx++] = wall_h;

        //test colors different walls and some side shades
        float r,g,b;
        if (hit.wall_t == 1) {
            r = 0.3f; g = 0.2f; b = 0.1f;
        } else if (hit.wall_t == 2) {
            r = 0.31f; g = 0.2f; b = 0.1f;
        } else {
            r = 0.32f; g = 0.1f; b = 0.2f;
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

        float fog_amount = hit.distance / fog_max_dist;
        if (fog_amount > 1.0f) fog_amount = 1.0f;
        r = r * (1.0f - fog_amount) + test_fog_r * fog_amount;
        g = g * (1.0f - fog_amount) + test_fog_g * fog_amount;
        b = b * (1.0f - fog_amount) + test_fog_b * fog_amount;


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

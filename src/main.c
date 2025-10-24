#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#define SOKOL_IMPL
#include "sokol/sokol_app.h"
#include "sokol/sokol_gfx.h"
#include "sokol/sokol_glue.h"
#include "sokol/sokol_log.h"


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MAP_W 16
#define MAP_H 16

#define FOV 1.147f //60degrees in rad

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
static sg_buffer uv_buf;


static sg_pipeline pip;
static sg_bindings r_bind;
static sg_image offscreen_img;
static sg_view offscreen_view;
static sg_view offscreen_texture_view;
static sg_pipeline display_pip;
static sg_bindings display_bind;
static sg_sampler display_sampler;


//texture test
static sg_image brick_texture;
static sg_sampler brick_sampler;
static sg_view brick_texture_view;

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
    float wall_hit_x;
} RayHit;


const char *vs_source =
"#version 330 core\n"
"layout(location=0) in vec2 position;\n"
"layout(location=1) in vec2 texcoord;\n"
"layout(location=2) in vec3 color;\n"
"out vec2 uv;\n"
"out vec3 tint;\n"
"void main() {\n"
"   gl_Position = vec4(position, 0.0, 1.0);\n" 
"   uv = texcoord;\n"
"   tint = color;\n"
"}\n";

const char *fs_source =
"#version 330 core\n"
"uniform sampler2D tex;\n"
"in vec2 uv;\n"
"in vec3 tint;\n"
"out vec4 frag_color;\n"
"void main() {\n"
"   vec4 tex_color = texture(tex, uv);\n"
"   frag_color = tex_color * vec4(tint, 1.0);\n"
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
"   // Chromatic aberration\n"
"   vec2 center = vec2(0.5, 0.5);\n"
"   vec2 offset = uv - center;\n"
"   float dist = length(offset);\n"
"   float aberration = 0.007* dist;\n"
"   vec2 dir = normalize(offset);\n"
"   \n"
"   float r = texture(tex, uv + dir * aberration * 2.0).r;\n"
"   float g = texture(tex, uv).g;\n"
"   float b = texture(tex, uv - dir * aberration * 2.0).b;\n"
"   \n"
"   vec3 color = vec3(r, g, b);\n"
"   \n"
"   // Random-looking dithering\n"
"   vec2 pixel = gl_FragCoord.xy;\n"
"   float noise = fract(sin(dot(pixel, vec2(12.9898, 78.233))) * 43758.5453);\n"
"   \n"
"   color += (noise - 0.5) * 0.04;\n"
"   \n"
"   frag_color = vec4(color, 1.0);\n"
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
            return (RayHit){100.0f, 0, 0, 0.0f};
        } 
        if (map[map_y][map_x] != 0) {
            float distance = (side == 1) ?  delta_x - delta_d_x : delta_y - delta_d_y;
            int wall_type = map[map_y][map_x];

                float wall_hit_x;
    if (side == 1) {  
        float hit_y = p_y + ray_dir_y * distance;
        wall_hit_x = hit_y - (int)hit_y;  
    } else {  
        float hit_x = p_x + ray_dir_x * distance;
        wall_hit_x = hit_x - (int)hit_x;  
    }
            return (RayHit){distance, wall_type, side, wall_hit_x};
        }
    }
    return (RayHit){100.0f, 0, 0, 0.0f};
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
            return (RayHit){100.0f, 0, 0, 0.0f};
        }
        if (map[(int)ray_y][(int)ray_x] != 0) {
            float dx = ray_x - p_x;
            float dy = ray_y - p_y;
            float dist = sqrtf(dx*dx + dy*dy);
            float fr_x = ray_x - map_x;
            float fr_y = ray_y - map_y;
            float wall_hit_x;
            if (fr_x  < 0.1 || fr_x > 0.9) {
                side = 1;
                wall_hit_x = fr_y;
            } else {
                side = 0;
                wall_hit_x = fr_x;
            }
            int wall_t = map[map_y][map_x];
            return (RayHit){dist, wall_t, side, wall_hit_x};
        }
    }
    return (RayHit){100.0f, 0, 0, 0.0f};
}


void init(void) {

    sg_setup(&(sg_desc) {
            .environment = sglue_environment(),
            .logger.func = slog_func,
            });

    //load textoo
    int tex_w, tex_h, tex_chann;
    unsigned char *tex_data = stbi_load("assets/textures/brick2.png",
                                        &tex_w, &tex_h, &tex_chann, 4);
    if (!tex_data) {
        printf("ERROR: Failed to load textoo Check file path.\n");
        printf("Looking for: assets/textures/brick.png\n");
        exit(1);  
    }

    printf("Textoo loaded!!: %dx%d, %d channels\n", tex_w, tex_h, tex_chann);
    
    brick_texture = sg_make_image(&(sg_image_desc){
            .width = tex_w,
            .height = tex_h,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .data.mip_levels[0] = {
                .ptr = tex_data,
                .size = (size_t)(tex_w * tex_h * 4)
            }
    });

    stbi_image_free(tex_data);

    brick_texture_view = sg_make_view(&(sg_view_desc){
            .texture.image = brick_texture
            });
    brick_sampler = sg_make_sampler(&(sg_sampler_desc){
            .min_filter = SG_FILTER_NEAREST,
            .mag_filter = SG_FILTER_NEAREST,
            .wrap_u = SG_WRAP_REPEAT,
            .wrap_v = SG_WRAP_REPEAT
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

    uv_buf = sg_make_buffer(&(sg_buffer_desc){
            .usage = {
            .vertex_buffer = true,
            .dynamic_update = true,
            },
            .size = sizeof(float) * 2 * 18 * 480,  // 2 floats per UV
    });

    sg_shader shad = sg_make_shader(&(sg_shader_desc){
            .vertex_func = { .source = vs_source},
            .fragment_func = {.source = fs_source},
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

    pip = sg_make_pipeline(&(sg_pipeline_desc){
            .shader = shad,
            .layout = {
            .attrs[0] = {
                .format = SG_VERTEXFORMAT_FLOAT2,
                .buffer_index = 0
            },
            .attrs[1] = {
                .format = SG_VERTEXFORMAT_FLOAT2,
                .buffer_index = 1
            },
            .attrs[2] = {
                .format = SG_VERTEXFORMAT_FLOAT3,
                .buffer_index = 2
            }
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_NONE,
            .write_enabled = false,
            .compare = SG_COMPAREFUNC_ALWAYS,
        },
    });
    r_bind.vertex_buffers[0] = pos_buf;
    r_bind.vertex_buffers[1] = uv_buf;
    r_bind.vertex_buffers[2] = col_buf;
    r_bind.views[0] = brick_texture_view;
    r_bind.samplers[0] = brick_sampler;

}

void frame(void) {


    float move_speed = 0.025f;
    float rot_speed = 0.025f;
    float min_wall_dist = 0.2f;

    float new_x = p_x;
    float new_y = p_y;

    if (key_w) {
        new_x += cosf(p_angle) * move_speed;
        new_y += sinf(p_angle) * move_speed;
    }
    if (key_s) {
        new_x -= cosf(p_angle) * move_speed;
        new_y -= sinf(p_angle) * move_speed;
    }
    if (key_a) {
        new_x += cosf(p_angle - 1.57f) * move_speed;
        new_y += sinf(p_angle - 1.57f) * move_speed;
    }
    if (key_d) {
        new_x += cosf(p_angle + 1.57f) * move_speed;
        new_y += sinf(p_angle + 1.57f) * move_speed;
    }

    bool can_move_both = true;
    bool can_move_x = true;
    bool can_move_y = true;

    float checks_both[][2] = {
        {new_x + min_wall_dist, new_y + min_wall_dist},
        {new_x - min_wall_dist, new_y + min_wall_dist},
        {new_x + min_wall_dist, new_y - min_wall_dist},
        {new_x - min_wall_dist, new_y - min_wall_dist}
    };

    float checks_x[][2] = {
        {new_x + min_wall_dist, p_y + min_wall_dist},
        {new_x - min_wall_dist, p_y + min_wall_dist},
        {new_x + min_wall_dist, p_y - min_wall_dist},
        {new_x - min_wall_dist, p_y - min_wall_dist}
    };

    float checks_y[][2] = {
        {p_x + min_wall_dist, new_y + min_wall_dist},
        {p_x - min_wall_dist, new_y + min_wall_dist},
        {p_x + min_wall_dist, new_y - min_wall_dist},
        {p_x - min_wall_dist, new_y - min_wall_dist}
    };

    for (int i = 0; i < 4; i++) {
        int cx = (int)checks_both[i][0];
        int cy = (int)checks_both[i][1];
        if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
            if (map[cy][cx] != 0) {
                can_move_both = false;
                break;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        int cx = (int)checks_x[i][0];
        int cy = (int)checks_x[i][1];
        if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
            if (map[cy][cx] != 0) {
                can_move_x = false;
                break;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        int cx = (int)checks_y[i][0];
        int cy = (int)checks_y[i][1];
        if (cx >= 0 && cx < MAP_W && cy >= 0 && cy < MAP_H) {
            if (map[cy][cx] != 0) {
                can_move_y = false;
                break;
            }
        }
    }

    if (can_move_both) {
        p_x = new_x;
        p_y = new_y;
    } else if (can_move_x) {
        p_x = new_x;
    } else if (can_move_y) {
        p_y = new_y;
    }

    if (key_q) p_angle -= rot_speed;
    if (key_e) p_angle += rot_speed;



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

    float plane_x = 0.0f;
    float plane_y = 0.65f; //fov
    float positions[480*18*2];
    float colors[480*18*3];
    float uvs[480*18*2];
    int pos_idx = 0;
    int col_idx = 0;
    int uv_idx = 0;

    for (int i = 0; i < 480; i++) {

        float screen_x = (2.0f * i / 480.0f) - 1.0f;
        float ray_angle = p_angle + screen_x * (FOV / 2.0f);

        RayHit hit = cast_ray(ray_angle);
        float p_distance = hit.distance * cosf(ray_angle - p_angle);

        float wall_x = hit.wall_hit_x;

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

        // Ceiling UVs
        for (int v = 0; v < 6; v++) {
            uvs[uv_idx++] = 0.75f;
            uvs[uv_idx++] = 0.75f;
        }

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

        //floor UV
        for (int v = 0; v < 6; v++) {
            uvs[uv_idx++] = 0.75f;
            uvs[uv_idx++] = 0.75f;
        }

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

        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 1.0f;
        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 0.0f;
        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 0.0f;
        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 1.0f;
        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 0.0f;
        uvs[uv_idx++] = wall_x; uvs[uv_idx++] = 1.0f;

        //test colors different walls and some side shades
        float r,g,b;
        if (hit.wall_t == 1) {
            r = 1.0f; g = 1.0f; b = 1.0f;
        } else if (hit.wall_t == 2) {
            r = 1.21f; g = 0.2f; b = 0.32f;
        } else {
            r = 1.0; g = 0.2f; b = 0.33f;
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

    sg_update_buffer(uv_buf, &(sg_range){
            .ptr = uvs,
            .size = uv_idx * sizeof(float)
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
            .clear_value = {1.0f, 0.0f, 0.0f, 1.0f}
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

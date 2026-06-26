#ifndef GRAPHICS_RENDERER_H
#define GRAPHICS_RENDERER_H

#include <stdint.h>
#include "drivers/video/gpu.h"

typedef struct { vec3_t pos; vec3_t normal; } mesh_vert_t;

typedef struct {
    mesh_vert_t v[3];
    uint32_t color;
} mesh_tri_t;

typedef struct {
    uint32_t* zbuffer;
    uint32_t width;
    uint32_t height;
} renderer_t;

void renderer_init(renderer_t* r, uint32_t* zbuf, uint32_t w, uint32_t h);
void renderer_clear(renderer_t* r, uint32_t color);
void renderer_draw_triangle(renderer_t* r, mesh_tri_t* tri);
void renderer_draw_mesh(renderer_t* r, float* mvp, mesh_tri_t* tris, int count, uint32_t base_color);

void mat4_identity(float* out);
void mat4_perspective(float* out, float fov, float aspect, float near, float far);
void mat4_lookat(float* out, vec3_t eye, vec3_t center, vec3_t up);
void mat4_rotate_y(float* out, float angle);
void mat4_rotate_x(float* out, float angle);
void mat4_translate(float* out, float x, float y, float z);
void mat4_mul(float* out, float* a, float* b);
void mat4_transform_vec4(float* out, float* m, vec3_t in);

#endif

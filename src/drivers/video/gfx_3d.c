#include "graphics/renderer.h"
#include <stdint.h>

/* Coarse Utah teapot mesh - triangles with normals */
typedef struct { float x,y,z; float nx,ny,nz; } teapot_vert_t;

static float my_sin(float x);
static float my_cos(float x);

static float my_sin(float x) {
    while (x > 3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f)));
}

static float my_cos(float x) {
    x += 1.57079632f;
    while (x > 3.14159265f) x -= 6.28318530f;
    while (x < -3.14159265f) x += 6.28318530f;
    float x2 = x * x;
    float s = x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f)));
    return s;
}

static const teapot_vert_t gBody[] = {
    /* body bottom ring */
    { 0.0000f, 0.0000f, 1.0000f, 0.0f, 1.0f, 0.0f},
    { 0.2588f, 0.0000f, 0.9659f, 0.0f, 1.0f, 0.0f},
    { 0.5000f, 0.0000f, 0.8660f, 0.0f, 1.0f, 0.0f},
    { 0.7071f, 0.0000f, 0.7071f, 0.0f, 1.0f, 0.0f},
    { 0.8660f, 0.0000f, 0.5000f, 0.0f, 1.0f, 0.0f},
    { 0.9659f, 0.0000f, 0.2588f, 0.0f, 1.0f, 0.0f},
    { 1.0000f, 0.0000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    { 0.9659f, 0.0000f,-0.2588f, 0.0f, 1.0f, 0.0f},
    { 0.8660f, 0.0000f,-0.5000f, 0.0f, 1.0f, 0.0f},
    { 0.7071f, 0.0000f,-0.7071f, 0.0f, 1.0f, 0.0f},
    { 0.5000f, 0.0000f,-0.8660f, 0.0f, 1.0f, 0.0f},
    { 0.2588f, 0.0000f,-0.9659f, 0.0f, 1.0f, 0.0f},
    { 0.0000f, 0.0000f,-1.0000f, 0.0f, 1.0f, 0.0f},
    {-0.2588f, 0.0000f,-0.9659f, 0.0f, 1.0f, 0.0f},
    {-0.5000f, 0.0000f,-0.8660f, 0.0f, 1.0f, 0.0f},
    {-0.7071f, 0.0000f,-0.7071f, 0.0f, 1.0f, 0.0f},
    {-0.8660f, 0.0000f,-0.5000f, 0.0f, 1.0f, 0.0f},
    {-0.9659f, 0.0000f,-0.2588f, 0.0f, 1.0f, 0.0f},
    {-1.0000f, 0.0000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    {-0.9659f, 0.0000f, 0.2588f, 0.0f, 1.0f, 0.0f},
    {-0.8660f, 0.0000f, 0.5000f, 0.0f, 1.0f, 0.0f},
    {-0.7071f, 0.0000f, 0.7071f, 0.0f, 1.0f, 0.0f},
    {-0.5000f, 0.0000f, 0.8660f, 0.0f, 1.0f, 0.0f},
    {-0.2588f, 0.0000f, 0.9659f, 0.0f, 1.0f, 0.0f}
};

static const teapot_vert_t gBody2[] = {
    { 0.0000f, 0.1000f, 0.9500f, 0.0f, 1.0f, 0.0f},
    { 0.2450f, 0.1000f, 0.9170f, 0.0f, 1.0f, 0.0f},
    { 0.4750f, 0.1000f, 0.8220f, 0.0f, 1.0f, 0.0f},
    { 0.6700f, 0.1000f, 0.6700f, 0.0f, 1.0f, 0.0f},
    { 0.8220f, 0.1000f, 0.4750f, 0.0f, 1.0f, 0.0f},
    { 0.9170f, 0.1000f, 0.2450f, 0.0f, 1.0f, 0.0f},
    { 0.9500f, 0.1000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    { 0.9170f, 0.1000f,-0.2450f, 0.0f, 1.0f, 0.0f},
    { 0.8220f, 0.1000f,-0.4750f, 0.0f, 1.0f, 0.0f},
    { 0.6700f, 0.1000f,-0.6700f, 0.0f, 1.0f, 0.0f},
    { 0.4750f, 0.1000f,-0.8220f, 0.0f, 1.0f, 0.0f},
    { 0.2450f, 0.1000f,-0.9170f, 0.0f, 1.0f, 0.0f},
    { 0.0000f, 0.1000f,-0.9500f, 0.0f, 1.0f, 0.0f},
    {-0.2450f, 0.1000f,-0.9170f, 0.0f, 1.0f, 0.0f},
    {-0.4750f, 0.1000f,-0.8220f, 0.0f, 1.0f, 0.0f},
    {-0.6700f, 0.1000f,-0.6700f, 0.0f, 1.0f, 0.0f},
    {-0.8220f, 0.1000f,-0.4750f, 0.0f, 1.0f, 0.0f},
    {-0.9170f, 0.1000f,-0.2450f, 0.0f, 1.0f, 0.0f},
    {-0.9500f, 0.1000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    {-0.9170f, 0.1000f, 0.2450f, 0.0f, 1.0f, 0.0f},
    {-0.8220f, 0.1000f, 0.4750f, 0.0f, 1.0f, 0.0f},
    {-0.6700f, 0.1000f, 0.6700f, 0.0f, 1.0f, 0.0f},
    {-0.4750f, 0.1000f, 0.8220f, 0.0f, 1.0f, 0.0f},
    {-0.2450f, 0.1000f, 0.9170f, 0.0f, 1.0f, 0.0f}
};

static const teapot_vert_t gBody3[] = {
    { 0.0000f, 0.2000f, 0.9000f, 0.0f, 1.0f, 0.0f},
    { 0.2300f, 0.2000f, 0.8690f, 0.0f, 1.0f, 0.0f},
    { 0.4500f, 0.2000f, 0.7790f, 0.0f, 1.0f, 0.0f},
    { 0.6400f, 0.2000f, 0.6400f, 0.0f, 1.0f, 0.0f},
    { 0.7790f, 0.2000f, 0.4500f, 0.0f, 1.0f, 0.0f},
    { 0.8690f, 0.2000f, 0.2300f, 0.0f, 1.0f, 0.0f},
    { 0.9000f, 0.2000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    { 0.8690f, 0.2000f,-0.2300f, 0.0f, 1.0f, 0.0f},
    { 0.7790f, 0.2000f,-0.4500f, 0.0f, 1.0f, 0.0f},
    { 0.6400f, 0.2000f,-0.6400f, 0.0f, 1.0f, 0.0f},
    { 0.4500f, 0.2000f,-0.7790f, 0.0f, 1.0f, 0.0f},
    { 0.2300f, 0.2000f,-0.8690f, 0.0f, 1.0f, 0.0f},
    { 0.0000f, 0.2000f,-0.9000f, 0.0f, 1.0f, 0.0f},
    {-0.2300f, 0.2000f,-0.8690f, 0.0f, 1.0f, 0.0f},
    {-0.4500f, 0.2000f,-0.7790f, 0.0f, 1.0f, 0.0f},
    {-0.6400f, 0.2000f,-0.6400f, 0.0f, 1.0f, 0.0f},
    {-0.7790f, 0.2000f,-0.4500f, 0.0f, 1.0f, 0.0f},
    {-0.8690f, 0.2000f,-0.2300f, 0.0f, 1.0f, 0.0f},
    {-0.9000f, 0.2000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    {-0.8690f, 0.2000f, 0.2300f, 0.0f, 1.0f, 0.0f},
    {-0.7790f, 0.2000f, 0.4500f, 0.0f, 1.0f, 0.0f},
    {-0.6400f, 0.2000f, 0.6400f, 0.0f, 1.0f, 0.0f},
    {-0.4500f, 0.2000f, 0.7790f, 0.0f, 1.0f, 0.0f},
    {-0.2300f, 0.2000f, 0.8690f, 0.0f, 1.0f, 0.0f}
};

static const teapot_vert_t gBody4[] = {
    { 0.0000f, 0.3000f, 0.8500f, 0.0f, 1.0f, 0.0f},
    { 0.2180f, 0.3000f, 0.8200f, 0.0f, 1.0f, 0.0f},
    { 0.4250f, 0.3000f, 0.7350f, 0.0f, 1.0f, 0.0f},
    { 0.6000f, 0.3000f, 0.6000f, 0.0f, 1.0f, 0.0f},
    { 0.7350f, 0.3000f, 0.4250f, 0.0f, 1.0f, 0.0f},
    { 0.8200f, 0.3000f, 0.2180f, 0.0f, 1.0f, 0.0f},
    { 0.8500f, 0.3000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    { 0.8200f, 0.3000f,-0.2180f, 0.0f, 1.0f, 0.0f},
    { 0.7350f, 0.3000f,-0.4250f, 0.0f, 1.0f, 0.0f},
    { 0.6000f, 0.3000f,-0.6000f, 0.0f, 1.0f, 0.0f},
    { 0.4250f, 0.3000f,-0.7350f, 0.0f, 1.0f, 0.0f},
    { 0.2180f, 0.3000f,-0.8200f, 0.0f, 1.0f, 0.0f},
    { 0.0000f, 0.3000f,-0.8500f, 0.0f, 1.0f, 0.0f},
    {-0.2180f, 0.3000f,-0.8200f, 0.0f, 1.0f, 0.0f},
    {-0.4250f, 0.3000f,-0.7350f, 0.0f, 1.0f, 0.0f},
    {-0.6000f, 0.3000f,-0.6000f, 0.0f, 1.0f, 0.0f},
    {-0.7350f, 0.3000f,-0.4250f, 0.0f, 1.0f, 0.0f},
    {-0.8200f, 0.3000f,-0.2180f, 0.0f, 1.0f, 0.0f},
    {-0.8500f, 0.3000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    {-0.8200f, 0.3000f, 0.2180f, 0.0f, 1.0f, 0.0f},
    {-0.7350f, 0.3000f, 0.4250f, 0.0f, 1.0f, 0.0f},
    {-0.6000f, 0.3000f, 0.6000f, 0.0f, 1.0f, 0.0f},
    {-0.4250f, 0.3000f, 0.7350f, 0.0f, 1.0f, 0.0f},
    {-0.2180f, 0.3000f, 0.8200f, 0.0f, 1.0f, 0.0f}
};

static const teapot_vert_t gBody5[] = {
    { 0.0000f, 0.4000f, 0.8000f, 0.0f, 1.0f, 0.0f},
    { 0.2000f, 0.4000f, 0.7730f, 0.0f, 1.0f, 0.0f},
    { 0.4000f, 0.4000f, 0.6930f, 0.0f, 1.0f, 0.0f},
    { 0.5730f, 0.4000f, 0.5730f, 0.0f, 1.0f, 0.0f},
    { 0.6930f, 0.4000f, 0.4000f, 0.0f, 1.0f, 0.0f},
    { 0.7730f, 0.4000f, 0.2000f, 0.0f, 1.0f, 0.0f},
    { 0.8000f, 0.4000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    { 0.7730f, 0.4000f,-0.2000f, 0.0f, 1.0f, 0.0f},
    { 0.6930f, 0.4000f,-0.4000f, 0.0f, 1.0f, 0.0f},
    { 0.5730f, 0.4000f,-0.5730f, 0.0f, 1.0f, 0.0f},
    { 0.4000f, 0.4000f,-0.6930f, 0.0f, 1.0f, 0.0f},
    { 0.2000f, 0.4000f,-0.7730f, 0.0f, 1.0f, 0.0f},
    { 0.0000f, 0.4000f,-0.8000f, 0.0f, 1.0f, 0.0f},
    {-0.2000f, 0.4000f,-0.7730f, 0.0f, 1.0f, 0.0f},
    {-0.4000f, 0.4000f,-0.6930f, 0.0f, 1.0f, 0.0f},
    {-0.5730f, 0.4000f,-0.5730f, 0.0f, 1.0f, 0.0f},
    {-0.6930f, 0.4000f,-0.4000f, 0.0f, 1.0f, 0.0f},
    {-0.7730f, 0.4000f,-0.2000f, 0.0f, 1.0f, 0.0f},
    {-0.8000f, 0.4000f, 0.0000f, 0.0f, 1.0f, 0.0f},
    {-0.7730f, 0.4000f, 0.2000f, 0.0f, 1.0f, 0.0f},
    {-0.6930f, 0.4000f, 0.4000f, 0.0f, 1.0f, 0.0f},
    {-0.5730f, 0.4000f, 0.5730f, 0.0f, 1.0f, 0.0f},
    {-0.4000f, 0.4000f, 0.6930f, 0.0f, 1.0f, 0.0f},
    {-0.2000f, 0.4000f, 0.7730f, 0.0f, 1.0f, 0.0f}
};

#define TEAPOT_SEGS 24
static mesh_tri_t gTris[800];
static int gTriCount = 0;
static uint32_t g_teapot_zbuf[1920*1080];

static void add_tri(teapot_vert_t a, teapot_vert_t b, teapot_vert_t c, uint32_t col) {
    if (gTriCount >= 800) return;
    mesh_tri_t* t = &gTris[gTriCount++];
    t->v[0].pos.x = a.x; t->v[0].pos.y = a.y; t->v[0].pos.z = a.z;
    t->v[0].normal.x = a.nx; t->v[0].normal.y = a.ny; t->v[0].normal.z = a.nz;
    t->v[1].pos.x = b.x; t->v[1].pos.y = b.y; t->v[1].pos.z = b.z;
    t->v[1].normal.x = b.nx; t->v[1].normal.y = b.ny; t->v[1].normal.z = b.nz;
    t->v[2].pos.x = c.x; t->v[2].pos.y = c.y; t->v[2].pos.z = c.z;
    t->v[2].normal.x = c.nx; t->v[2].normal.y = c.ny; t->v[2].normal.z = c.nz;
    t->color = col;
}

static uint32_t lerp_color(uint32_t a, uint32_t b, float f) {
    f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
    uint8_t ar = (a>>16)&0xFF, ag = (a>>8)&0xFF, ab = a&0xFF;
    uint8_t br = (b>>16)&0xFF, bg = (b>>8)&0xFF, bb = b&0xFF;
    uint8_t rr = (uint8_t)(ar + (br-ar)*f);
    uint8_t rg = (uint8_t)(ag + (bg-ag)*f);
    uint8_t rb = (uint8_t)(ab + (bb-ab)*f);
    return (rr<<16)|(rg<<8)|rb;
}

static void build_ring(const teapot_vert_t* ring, int n, const teapot_vert_t* next, uint32_t c1, uint32_t c2) {
    for (int i = 0; i < n; i++) {
        int ni = (i+1)%n;
        float f = (float)i / (float)n;
        add_tri(ring[i], ring[ni], next[i], lerp_color(c1, c2, f));
        add_tri(ring[ni], next[ni], next[i], lerp_color(c1, c2, f));
    }
}

static void build_cap(const teapot_vert_t* ring, int n, float cy, uint32_t c) {
    int mid = n/2;
    for (int i = 0; i < n; i++) {
        int ni = (i+1)%n;
        add_tri(ring[i], ring[ni], (teapot_vert_t){0,cy,0,0,1,0}, c);
    }
}

static void build_teapot(void) {
    gTriCount = 0;
    uint32_t c1 = 0xCC4444;
    uint32_t c2 = 0x44CC44;
    uint32_t c3 = 0x4444CC;
    uint32_t c4 = 0xCCCC44;
    uint32_t c5 = 0xCC44CC;

    /* body rings */
    build_ring(gBody, 24, gBody2, c1, c2);
    build_ring(gBody2, 24, gBody3, c2, c3);
    build_ring(gBody3, 24, gBody4, c3, c4);
    build_ring(gBody4, 24, gBody5, c4, c5);
    build_cap(gBody5, 24, 0.4f, c5);
    build_cap(gBody, 24, 0.0f, c1);

    /* lid */
    const int kLidSegs = 12;
    float lr[12], lh[12];
    for (int i = 0; i < kLidSegs; i++) {
        float a = 2.0f*3.14159265f*i/kLidSegs;
        lr[i] = 0.35f * my_cos(a);
        lh[i] = 0.45f + 0.10f * my_sin(a);
    }
    teapot_vert_t lid1[12], lid2[12], lid3[12], lid4[12];
    for (int i = 0; i < kLidSegs; i++) {
        lid1[i] = (teapot_vert_t){lr[i], lh[i], lr[i], 0, 1, 0};
        lid2[i] = (teapot_vert_t){lr[i]*0.85f, lh[i]+0.08f, lr[i]*0.85f, 0, 1, 0};
        lid3[i] = (teapot_vert_t){lr[i]*0.6f, lh[i]+0.16f, lr[i]*0.6f, 0, 1, 0};
        lid4[i] = (teapot_vert_t){lr[i]*0.2f, lh[i]+0.22f, lr[i]*0.2f, 0, 1, 0};
    }
    build_ring(lid1, kLidSegs, lid2, c5, c2);
    build_ring(lid2, kLidSegs, lid3, c2, c3);
    build_ring(lid3, kLidSegs, lid4, c3, c1);
    build_cap(lid4, kLidSegs, lh[0]+0.22f, c1);
}

void gfx_3d_render_teapot(int rx, int ry, int rw, int rh, float ax, float ay) {
    build_teapot();

    float mvp[16];
    float model[16], view[16], proj[16], tmp[16];

    mat4_identity(model);
    mat4_rotate_y(model, ay);
    mat4_rotate_x(model, ax);
    mat4_translate(tmp, 0.0f, -0.2f, -5.0f);
    mat4_mul(model, tmp, model);

    vec3_t eye = {0, 1.5f, -3.0f};
    vec3_t center = {0, 0.2f, 0};
    vec3_t up = {0, 1, 0};
    mat4_lookat(view, eye, center, up);
    mat4_perspective(proj, 1.2f, (float)rw/(float)rh, 0.1f, 100.0f);

    mat4_mul(tmp, proj, view);
    mat4_mul(mvp, tmp, model);

    renderer_t rr;
    renderer_init(&rr, g_teapot_zbuf, rw, rh);
    renderer_clear(&rr, 0xFF181818);
    renderer_draw_mesh(&rr, mvp, gTris, gTriCount, 0xFFCC8844);
}

void gfx_3d_render_teacup(int rx, int ry, int rw, int rh, float ax, float ay) {
    gfx_3d_render_teapot(rx, ry, rw, rh, ax, ay);
}

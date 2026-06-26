#include "graphics/renderer.h"
#include "drivers/video/framebuffer.h"
#include <string.h>

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float my_sin(float x);
static float my_cos(float x);
static float my_tan(float x);

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

static float my_tan(float x) {
    float c = my_cos(x);
    if (c < 0.0001f && c > -0.0001f) return 10.0f;
    return my_sin(x) / c;
}

/* ----- matrix ops ----- */
void mat4_identity(float* out) {
    for (int i = 0; i < 16; i++) out[i] = 0.0f;
    out[0] = out[5] = out[10] = out[15] = 1.0f;
}

void mat4_perspective(float* out, float fov, float aspect, float near, float far) {
    for (int i = 0; i < 16; i++) out[i] = 0.0f;
    float f = 1.0f / my_tan(fov * 0.5f);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (far + near) / (near - far);
    out[11] = -1.0f;
    out[14] = (2.0f * far * near) / (near - far);
}

void mat4_lookat(float* out, vec3_t eye, vec3_t center, vec3_t up) {
    vec3_t f = { center.x - eye.x, center.y - eye.y, center.z - eye.z };
    float len = 1.0f / (f.x*f.x + f.y*f.y + f.z*f.z);
    f.x *= len; f.y *= len; f.z *= len;

    vec3_t s = {
        f.y * up.z - f.z * up.y,
        f.z * up.x - f.x * up.z,
        f.x * up.y - f.y * up.x
    };
    len = 1.0f / (s.x*s.x + s.y*s.y + s.z*s.z);
    s.x *= len; s.y *= len; s.z *= len;

    vec3_t u = {
        s.y * f.z - s.z * f.y,
        s.z * f.x - s.x * f.z,
        s.x * f.y - s.y * f.x
    };

    mat4_identity(out);
    out[0] = s.x; out[1] = u.x; out[2] = -f.x;
    out[4] = s.y; out[5] = u.y; out[6] = -f.y;
    out[8] = s.z; out[9] = u.z; out[10] = -f.z;
    out[12] = -(s.x*eye.x + s.y*eye.y + s.z*eye.z);
    out[13] = -(u.x*eye.x + u.y*eye.y + u.z*eye.z);
    out[14] = (f.x*eye.x + f.y*eye.y + f.z*eye.z);
}

void mat4_rotate_y(float* out, float angle) {
    mat4_identity(out);
    float c = my_cos(angle), s = my_sin(angle);
    out[0] = c; out[2] = s;
    out[8] = -s; out[10] = c;
}

void mat4_rotate_x(float* out, float angle) {
    mat4_identity(out);
    float c = my_cos(angle), s = my_sin(angle);
    out[5] = c; out[6] = -s;
    out[9] = s; out[10] = c;
}

void mat4_translate(float* out, float x, float y, float z) {
    mat4_identity(out);
    out[12] = x; out[13] = y; out[14] = z;
}

void mat4_mul(float* out, float* a, float* b) {
    float r[16];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            r[j*4+i] = 0.0f;
            for (int k = 0; k < 4; k++) r[j*4+i] += a[k*4+i] * b[j*4+k];
        }
    }
    for (int i = 0; i < 16; i++) out[i] = r[i];
}

void mat4_transform_vec4(float* out, float* m, vec3_t in) {
    out[0] = m[0]*in.x + m[4]*in.y + m[8]*in.z + m[12];
    out[1] = m[1]*in.x + m[5]*in.y + m[9]*in.z + m[13];
    out[2] = m[2]*in.x + m[6]*in.y + m[10]*in.z + m[14];
    out[3] = m[3]*in.x + m[7]*in.y + m[11]*in.z + m[15];
}

/* ----- renderer ----- */
void renderer_init(renderer_t* r, uint32_t* zbuf, uint32_t w, uint32_t h) {
    r->zbuffer = zbuf;
    r->width = w;
    r->height = h;
}

void renderer_clear(renderer_t* r, uint32_t color) {
    uint32_t* fb = gfx_get_back_buffer();
    uint32_t w = gfx_get_fb_width();
    uint32_t h = gfx_get_fb_height();
    for (uint32_t i = 0; i < w * h; i++) {
        fb[i] = color;
        r->zbuffer[i] = 0xFFFFFFFF;
    }
}

static uint32_t shade(vec3_t normal, uint32_t base, vec3_t light_dir) {
    float ndotl = normal.x*light_dir.x + normal.y*light_dir.y + normal.z*light_dir.z;
    float diffuse = clampf(ndotl, 0.0f, 1.0f);
    float ambient = 0.25f;
    float intensity = ambient + diffuse * 0.75f;

    uint8_t r = (uint8_t)(((base >> 16) & 0xFF) * intensity);
    uint8_t g = (uint8_t)(((base >> 8) & 0xFF) * intensity);
    uint8_t b = (uint8_t)((base & 0xFF) * intensity);
    return (r << 16) | (g << 8) | b;
}

void renderer_draw_triangle(renderer_t* r, mesh_tri_t* tri) {
    if (!r || !tri) return;
    float x0 = tri->v[0].pos.x, y0 = tri->v[0].pos.y, z0 = tri->v[0].pos.z;
    float x1 = tri->v[1].pos.x, y1 = tri->v[1].pos.y, z1 = tri->v[1].pos.z;
    float x2 = tri->v[2].pos.x, y2 = tri->v[2].pos.y, z2 = tri->v[2].pos.z;

    if (x0 == x1 && y0 == y1) return;
    if (x1 == x2 && y1 == y2) return;
    if (x0 == x2 && y0 == y2) return;

    int ix0 = (int)x0, iy0 = (int)y0, ix1 = (int)x1, iy1 = (int)y1, ix2 = (int)x2, iy2 = (int)y2;

    if (iy0 > iy1) { int tx=ix0, ty=iy0; ix0=ix1; iy0=iy1; ix1=tx; iy1=ty; }
    if (iy0 > iy2) { int tx=ix0, ty=iy0; ix0=ix2; iy0=iy2; ix2=tx; iy2=ty; }
    if (iy1 > iy2) { int tx=ix1, ty=iy1; ix1=ix2; iy1=iy2; ix2=tx; iy2=ty; }

    int total_h = iy2 - iy0;
    if (total_h == 0) return;

    for (int y = iy0; y <= iy2; y++) {
        if (y < 0 || y >= (int)r->height) continue;
        int seg_h = iy1 - iy0;
        int seg_h2 = iy2 - iy1;
        float t1 = (float)(y - iy0) / (float)total_h;
        float t2 = (seg_h == 0) ? 1.0f : (float)(y - iy0) / (float)seg_h;
        float t3 = (seg_h2 == 0) ? 1.0f : (float)(y - iy1) / (float)seg_h2;

        int ax, bx;
        if (y < iy1) {
            ax = ix0 + (int)((ix2 - ix0) * t1);
            bx = ix0 + (int)((ix1 - ix0) * t2);
        } else {
            ax = ix0 + (int)((ix2 - ix0) * t1);
            bx = ix1 + (int)((ix2 - ix1) * t3);
        }
        if (ax > bx) { int t = ax; ax = bx; bx = t; }
        if (ax < 0) ax = 0;
        if (bx >= (int)r->width) bx = r->width - 1;
        for (int x = ax; x <= bx; x++) {
            float u = 0.0f, v = 0.0f;
            if (bx > ax) { u = (float)(x - ax) / (float)(bx - ax); }
            float z = z0 + (z2 - z0)*t1;
            uint32_t idx = (uint32_t)y * r->width + x;
            if (z < r->zbuffer[idx]) {
                r->zbuffer[idx] = z;
                vec3_t n = {
                    tri->v[0].normal.x + (tri->v[2].normal.x - tri->v[0].normal.x)*u + (tri->v[1].normal.x - tri->v[0].normal.x)*(1.0f-u),
                    tri->v[0].normal.y + (tri->v[2].normal.y - tri->v[0].normal.y)*u + (tri->v[1].normal.y - tri->v[0].normal.y)*(1.0f-u),
                    tri->v[0].normal.z + (tri->v[2].normal.z - tri->v[0].normal.z)*u + (tri->v[1].normal.z - tri->v[0].normal.z)*(1.0f-u)
                };
                float nl = n.x*n.x + n.y*n.y + n.z*n.z;
                if (nl > 0.0001f) { float inv = 1.0f / nl; n.x*=inv; n.y*=inv; n.z*=inv; }
                put_pixel(x, y, shade(n, tri->color, (vec3_t){0.577f, -0.577f, 0.577f}));
            }
        }
    }
}

static void transform_vertex(vec3_t* out, float* m, vec3_t in) {
    float w = m[3]*in.x + m[7]*in.y + m[11]*in.z + m[15];
    out->x = m[0]*in.x + m[4]*in.y + m[8]*in.z + m[12];
    out->y = m[1]*in.x + m[5]*in.y + m[9]*in.z + m[13];
    out->z = m[2]*in.x + m[6]*in.y + m[10]*in.z + m[14];
    if (w > 0.0001f) { out->x /= w; out->y /= w; out->z /= w; }
}

void renderer_draw_mesh(renderer_t* r, float* mvp, mesh_tri_t* tris, int count, uint32_t base_color) {
    mesh_tri_t screen[512];
    if (count > 512) count = 512;
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < 3; j++) {
            transform_vertex(&screen[i].v[j].pos, mvp, tris[i].v[j].pos);
            screen[i].v[j].normal = tris[i].v[j].normal;
        }
        screen[i].color = tris[i].color;
        renderer_draw_triangle(r, &screen[i]);
    }
}

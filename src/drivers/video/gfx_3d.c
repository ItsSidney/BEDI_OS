#include "drivers/video/gpu.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/gui.h"

#define PI 3.14159265f

static float my_sin(float x);
static float my_cos(float x);
static vec3_t project(vec3_t v, float width, float height);
static vec3_t rotate_x(vec3_t v, float angle);
static vec3_t rotate_y(vec3_t v, float angle);
static vec3_t rotate_z(vec3_t v, float angle);

static float my_sin(float x) {
    while (x > PI) x -= 2 * PI;
    while (x < -PI) x += 2 * PI;
    float x2 = x * x;
    return x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f)));
}

static float my_cos(float x) {
    return my_sin(x + PI/2.0f);
}

void gfx_3d_init(void) {
}

static vec3_t project(vec3_t v, float width, float height) {
    float z_offset = 5.0f;
    float factor = 400.0f;
    float fov = factor / (v.z + z_offset);
    vec3_t p;
    p.x = (v.x * fov) + width / 2.0f;
    p.y = (v.y * fov) + height / 2.0f;
    p.z = v.z;
    return p;
}

static vec3_t rotate_x(vec3_t v, float angle) {
    float c = my_cos(angle);
    float s = my_sin(angle);
    vec3_t r = {v.x, v.y * c - v.z * s, v.y * s + v.z * c};
    return r;
}

static vec3_t rotate_y(vec3_t v, float angle) {
    float c = my_cos(angle);
    float s = my_sin(angle);
    vec3_t r = {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
    return r;
}

static vec3_t rotate_z(vec3_t v, float angle) {
    float c = my_cos(angle);
    float s = my_sin(angle);
    vec3_t r = {v.x * c - v.y * s, v.x * s + v.y * c, v.z};
    return r;
}

static void swap(int* a, int* b) { int t = *a; *a = *b; *b = t; }

static void draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t color, int cw, int ch) {
    if (y0 > y1) { swap(&x0, &x1); swap(&y0, &y1); }
    if (y0 > y2) { swap(&x0, &x2); swap(&y0, &y2); }
    if (y1 > y2) { swap(&x1, &x2); swap(&y1, &y2); }

    int total_h = y2 - y0;
    if (total_h == 0) return;

    uint32_t fb_h = get_fb_height();
    uint32_t fb_w = get_fb_width();

    for (int y = y0; y <= y2; y++) {
        if (y < 0 || y >= (int)fb_h) continue;
        int seg_h = y1 - y0;
        int seg_h2 = y2 - y1;
        float t1 = (total_h == 0) ? 0 : (float)(y - y0) / total_h;
        float t2 = (seg_h == 0) ? 1 : (float)(y - y0) / seg_h;
        float t3 = (seg_h2 == 0) ? 1 : (float)(y - y1) / seg_h2;

        int a_x, b_x;
        if (y < y1) {
            a_x = x0 + (x2 - x0) * t1;
            b_x = x0 + (x1 - x0) * t2;
        } else {
            a_x = x0 + (x2 - x0) * t1;
            b_x = x1 + (x2 - x1) * t3;
        }
        if (a_x > b_x) swap(&a_x, &b_x);
        if (a_x < 0) a_x = 0;
        if (b_x >= (int)fb_w) b_x = fb_w - 1;
        for (int x = a_x; x <= b_x; x++) {
            put_pixel(x, y, color);
        }
    }
}

#define TEACUP_SEGMENTS 24

static void draw_body_ring(vec3_t* p, int n, int offset, uint32_t color) {
    for (int i = 0; i < n; i++) {
        int ni = (i + 1) % n;
        gfx_draw_line((int)p[offset + i].x, (int)p[offset + i].y,
                      (int)p[offset + ni].x, (int)p[offset + ni].y, color);
    }
}

void gfx_3d_render_teacup(int rx, int ry, int rw, int rh, float ax, float ay) {
    int n = TEACUP_SEGMENTS;
    vec3_t verts[200];
    int vi = 0;

    float radii[] = {0.5f, 0.5f, 0.85f, 1.0f, 1.05f};
    float heights[] = {-1.3f, -1.0f, 0.0f, 1.0f, 1.2f};
    int rings = 5;

    for (int r = 0; r < rings; r++) {
        for (int i = 0; i < n; i++) {
            float ang = 2.0f * PI * i / n;
            verts[vi].x = radii[r] * my_cos(ang);
            verts[vi].y = heights[r];
            verts[vi].z = radii[r] * my_sin(ang);
            vi++;
        }
    }

    int handle_start = vi;
    for (int i = 0; i <= 6; i++) {
        float t = (float)i / 6.0f;
        float a = PI * 0.15f + t * PI * 0.7f;
        float hx = 0.9f + 0.8f * my_cos(a);
        float hy = 0.5f - 0.7f * my_sin(a);
        verts[vi].x = hx - 0.18f; verts[vi].y = hy; verts[vi].z = -0.12f; vi++;
        verts[vi].x = hx + 0.18f; verts[vi].y = hy; verts[vi].z = 0.12f; vi++;
    }

    vec3_t proj[200];
    for (int i = 0; i < vi; i++) {
        vec3_t v = verts[i];
        v = rotate_x(v, ax);
        v = rotate_y(v, ay);
        v = rotate_z(v, ax * 0.15f);
        vec3_t p = project(v, (float)rw, (float)rh);
        p.x += rx;
        p.y += ry;
        proj[i] = p;
    }

    uint32_t accent = get_accent_color();
    uint32_t dark_c = gfx_darken(accent, 50);
    uint32_t mid_c = accent;
    uint32_t handle_c = gfx_darken(accent, 25);
    uint32_t inner_c = 0x2A1F0E;

    for (int r = 0; r < rings - 1; r++) {
        for (int i = 0; i < n; i++) {
            int ni = (i + 1) % n;
            int a = r * n + i;
            int b = r * n + ni;
            int c = (r + 1) * n + i;
            int d = (r + 1) * n + ni;

            float t = (float)r / (rings - 2);
            uint32_t col = gfx_lerp_color(dark_c, mid_c, (int)(t * 100), 100);

            draw_filled_triangle(proj[a].x, proj[a].y, proj[b].x, proj[b].y, proj[c].x, proj[c].y, col, rw, rh);
            draw_filled_triangle(proj[b].x, proj[b].y, proj[c].x, proj[c].y, proj[d].x, proj[d].y, col, rw, rh);
        }
    }

    float bot_cx = 0, bot_cy = 0;
    for (int i = 0; i < n; i++) {
        bot_cx += proj[i].x; bot_cy += proj[i].y;
    }
    bot_cx /= n; bot_cy /= n;
    for (int i = 0; i < n; i++) {
        int ni = (i + 1) % n;
        draw_filled_triangle(proj[i].x, proj[i].y, proj[ni].x, proj[ni].y, bot_cx, bot_cy, dark_c, rw, rh);
    }

    int top_r = (rings - 1) * n;
    float top_cx = 0, top_cy = 0;
    for (int i = 0; i < n; i++) {
        top_cx += proj[top_r + i].x; top_cy += proj[top_r + i].y;
    }
    top_cx /= n; top_cy /= n;
    for (int i = 0; i < n; i++) {
        int ni = (i + 1) % n;
        draw_filled_triangle(proj[top_r + i].x, proj[top_r + i].y,
                             proj[top_r + ni].x, proj[top_r + ni].y,
                             top_cx, top_cy, inner_c, rw, rh);
    }

    for (int i = 0; i < 6; i++) {
        int a = handle_start + i * 2;
        int b = handle_start + i * 2 + 1;
        int c = handle_start + (i + 1) * 2;
        int d = handle_start + (i + 1) * 2 + 1;
        draw_filled_triangle(proj[a].x, proj[a].y, proj[b].x, proj[b].y, proj[c].x, proj[c].y, handle_c, rw, rh);
        draw_filled_triangle(proj[b].x, proj[b].y, proj[c].x, proj[c].y, proj[d].x, proj[d].y, handle_c, rw, rh);
    }

    uint32_t outline = gfx_darken(accent, 35);
    draw_body_ring(proj, n, 0, outline);
    draw_body_ring(proj, n, (rings - 1) * n, outline);
    for (int i = 0; i < n; i += 4) {
        int a = i;
        int b = (rings - 1) * n + i;
        gfx_draw_line((int)proj[a].x, (int)proj[a].y,
                      (int)proj[b].x, (int)proj[b].y, outline);
    }

    int cx2 = rx + rw / 2;
    int cy2 = ry + rh / 2;
    gfx_draw_line(cx2 - 20, cy2 - 8, cx2 - 8, cy2 - 20, 0x66FFFFFF);
}

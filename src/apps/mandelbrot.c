#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include <stdint.h>

#define MAX_ITER 80
#define RENDER_W 240
#define RENDER_H 180

static uint32_t mandel_buf[RENDER_W * RENDER_H];
static float mandel_phase = 0.0f;
static int mandel_zoom_dir = 1;

static uint32_t mandelbrot_color(int iter) {
    if (iter >= MAX_ITER) return 0x000000;
    int r = (iter * 9) & 0xFF;
    int g = (iter * 17) & 0xFF;
    int b = (iter * 23) & 0xFF;
    return (r << 16) | (g << 8) | b;
}

static void render_mandelbrot(void) {
    float cx_center = -0.75f;
    float cy_center = 0.0f;
    float zoom = 1.0f + mandel_phase * 2.5f;

    for (int py = 0; py < RENDER_H; py++) {
        for (int px = 0; px < RENDER_W; px++) {
            float x0 = cx_center + (px - RENDER_W / 2.0f) * 3.0f / (RENDER_W * zoom);
            float y0 = cy_center + (py - RENDER_H / 2.0f) * 3.0f / (RENDER_H * zoom);
            float x = 0.0f, y = 0.0f;
            int iter = 0;
            while (x * x + y * y <= 4.0f && iter < MAX_ITER) {
                float xt = x * x - y * y + x0;
                y = 2.0f * x * y + y0;
                x = xt;
                iter++;
            }
            mandel_buf[py * RENDER_W + px] = mandelbrot_color(iter);
        }
    }
}

static void mandelbrot_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;

    mandel_phase += 0.008f * mandel_zoom_dir;
    if (mandel_phase > 1.0f) { mandel_phase = 1.0f; mandel_zoom_dir = -1; }
    if (mandel_phase < 0.0f) { mandel_phase = 0.0f; mandel_zoom_dir = 1; }

    render_mandelbrot();
    gfx_draw_sprite_scaled(x, y, w, h, mandel_buf, RENDER_W, RENDER_H);
}

static void mandelbrot_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
    mandel_phase = 0.0f;
    mandel_zoom_dir = 1;
}

void mandelbrot_app(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw - 520) / 2, (fh - 440) / 2, 520, 440,
                    "Mandelbrot", 0x8AB4F8, mandelbrot_render, 0, mandelbrot_on_resize);
}

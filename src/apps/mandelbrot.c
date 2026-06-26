#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include <stdint.h>

#define MAX_ITER 64

static uint32_t mandel_buf[160 * 160];
static int mandel_frame = 0;
static int mandel_win_id = -1;

static uint32_t mandelbrot_color(int iter) {
    if (iter >= MAX_ITER) return 0x000000;
    int r = (iter * 7) & 0xFF;
    int g = (iter * 13) & 0xFF;
    int b = (iter * 19) & 0xFF;
    return (r << 16) | (g << 8) | b;
}

static void render_mandelbrot(int w, int h) {
    int sw = w / 3;
    int sh = h / 3;
    if (sw > 160) sw = 160;
    if (sh > 160) sh = 160;

    float cx_center = -0.75f;
    float cy_center = 0.0f;
    float zoom = 1.0f + mandel_frame * 0.02f;

    for (int py = 0; py < sh; py++) {
        for (int px = 0; px < sw; px++) {
            float x0 = cx_center + (px - sw / 2.0f) * 3.0f / (sw * zoom);
            float y0 = cy_center + (py - sh / 2.0f) * 3.0f / (sh * zoom);
            float x = 0.0f, y = 0.0f;
            int iter = 0;
            while (x * x + y * y <= 4.0f && iter < MAX_ITER) {
                float xt = x * x - y * y + x0;
                y = 2.0f * x * y + y0;
                x = xt;
                iter++;
            }
            mandel_buf[py * sw + px] = mandelbrot_color(iter);
        }
    }

    gfx_draw_sprite_scaled(0, 0, w, h, mandel_buf, sw, sh);
}

static void mandelbrot_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    render_mandelbrot(w, h);
    mandel_frame++;
}

static void mandelbrot_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
    mandel_frame = 0;
}

void mandelbrot_app(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    mandel_win_id = wm_open_window((fw - 500) / 2, (fh - 420) / 2, 500, 420,
                                    "Mandelbrot", 0x8AB4F8, mandelbrot_render, 0, mandelbrot_on_resize);
}

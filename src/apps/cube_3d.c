#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "kernel/time/timer.h"

extern void gfx_3d_render_teapot(int x, int y, int w, int h, float ax, float ay);

static float angle_x = 0;
static float angle_y = 0;

static void teapot_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x111111);
    angle_x += 0.03f;
    angle_y += 0.015f;
    gfx_3d_render_teapot(x, y, w, h, angle_x, angle_y);
}

static void teapot_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

void cube_3d_app(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    wm_open_window((fw-550)/2, (fh-550)/2, 550, 550, "Teapot", 0x8AB4F8, teapot_render, 0, teapot_on_resize);
}

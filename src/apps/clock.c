#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/time/rtc.h"

static int sin_tab[91];

static void init_sin(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    for (int i = 0; i <= 90; i++) {
        float rad = i * 3.14159f / 180.0f;
        float x = rad;
        float x2 = x * x;
        sin_tab[i] = (int)(65536.0f * x * (1.0f - x2 * (1.0f/6.0f - x2 * (1.0f/120.0f - x2 * (1.0f/5040.0f)))));
    }
}

static int fsin(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    if (deg <= 90) return sin_tab[deg];
    if (deg <= 180) return sin_tab[180 - deg];
    if (deg <= 270) return -sin_tab[deg - 180];
    return -sin_tab[360 - deg];
}

static int fcos(int deg) { return fsin(90 - deg); }

static void draw_hand(int cx, int cy, int len, int deg, uint32_t color) {
    int x = cx + (len * fsin(deg)) / 65536;
    int y = cy - (len * fcos(deg)) / 65536;
    gfx_draw_line(cx, cy, x, y, color);
}

static void clock_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0D1A);
    init_sin();

    time_t t;
    get_time(&t);

    int cx = x + w / 2;
    int cy = y + h / 2;
    int r = (w < h ? w : h) / 2 - 16;

    gfx_draw_rect_outline(x + 1, y + 1, w - 2, h - 2, 1, 0x2A2A4A);

    static const char* nums[] = {"12","1","2","3","4","5","6","7","8","9","10","11"};
    for (int i = 0; i < 12; i++) {
        int deg = i * 30 - 90;
        int nx = cx + (r - 16) * fsin(deg + 90) / 65536;
        int ny = cy - (r - 16) * fcos(deg + 90) / 65536;
        const char* s = nums[i];
        int sw = gfx_strlen(s) * 8;
        gfx_draw_string_transparent(nx - sw / 2, ny - 4, s, 0x8AB4F8);
    }

    int h_deg = (t.hour % 12) * 30 + t.minute / 2;
    int m_deg = t.minute * 6;
    int s_deg = t.second * 6;

    draw_hand(cx, cy, r * 45 / 100, h_deg, 0x8AB4F8);
    draw_hand(cx, cy, r * 65 / 100, m_deg, 0x34D399);
    draw_hand(cx, cy, r * 80 / 100, s_deg, 0xF87171);

    gfx_fill_circle(cx, cy, 3, 0xFFFFFF);
    gfx_fill_circle(cx, cy, 2, 0x111111);
}

static void clock_on_key(int id, char key) {
    (void)id; (void)key;
}

void clock_app(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int s = 260;
    wm_open_window((fw - s) / 2, (fh - s) / 2, s, s,
                   "Clock", 0x8AB4F8, clock_render, clock_on_key, 0);
}

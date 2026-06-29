#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "kernel/log.h"
#include "drivers/video/framebuffer.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int kl_win_id = -1;
static int kl_scroll = 0;
static int kl_line_count = 0;
static int kl_line_starts[256];

#define KL_ROW_H 16
#define KL_HEADER_H 24

static void kl_build_lines(void) {
    kl_line_count = 0;
    kl_line_starts[0] = 0;
    const char* buf = log_get_buffer();
    int size = log_get_size();
    if (!buf || size == 0) {
        kl_line_count = 1;
        kl_line_starts[0] = 0;
        return;
    }
    for (int i = 0; i < size && kl_line_count < 254; i++) {
        if (buf[i] == '\n') {
            kl_line_count++;
            kl_line_starts[kl_line_count] = i + 1;
        }
    }
    if (size > 0) kl_line_count++;
    if (kl_line_count == 0) kl_line_count = 1;
}

static void kl_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    uint32_t bg = 0x0D1117;
    uint32_t header_bg = 0x161B22;
    uint32_t border = 0x30363D;
    uint32_t text = 0xC9D1D9;
    uint32_t dim = 0x6E7681;
    uint32_t accent = get_accent_color();

    gfx_fill_rect(x, y, w, h, bg);

    gfx_fill_rect(x, y, w, KL_HEADER_H, header_bg);
    gfx_draw_hline(x, y + KL_HEADER_H, w, border);
    gfx_draw_string_transparent(x + 8, y + 4, "Kernel Boot Log", 0xFFFFFF);

    char buf[32];
    itoa(kl_line_count, buf);
    gfx_draw_string_transparent(x + w - 80, y + 4, buf, dim);
    gfx_draw_string_transparent(x + w - 96, y + 4, "Lines: ", dim);

    int body_y = y + KL_HEADER_H + 2;
    int body_h = h - KL_HEADER_H - 4;
    int rows_vis = body_h / KL_ROW_H;

    int total = kl_line_count;
    if (total < 1) total = 1;

    int sb_x = x + w - 12;
    gfx_fill_rect(sb_x, body_y, 12, body_h, 0x161B22);
    gfx_draw_vline(sb_x, body_y, body_h, border);
    gfx_draw_vline(sb_x + 11, body_y, body_h, border);

    int thumb_h = (body_h * rows_vis) / total;
    if (thumb_h < 14) thumb_h = 14;
    int max_scroll = total - rows_vis;
    if (max_scroll < 0) max_scroll = 0;
    if (max_scroll > 0) {
        int thumb_y = body_y + (kl_scroll * (body_h - thumb_h)) / max_scroll;
        gfx_fill_rect(sb_x + 1, thumb_y, 10, thumb_h, 0x30363D);
    }

    const char* log_buf = log_get_buffer();
    int log_size = log_get_size();

    for (int i = 0; i < rows_vis; i++) {
        int line_idx = i + kl_scroll;
        if (line_idx >= kl_line_count) break;

        int ry = body_y + i * KL_ROW_H;
        if (i % 2 == 0)
            gfx_fill_rect(x, ry, w - 14, KL_ROW_H, bg);
        else
            gfx_fill_rect(x, ry, w - 14, KL_ROW_H, 0x111820);

        int start = kl_line_starts[line_idx];
        int len = 0;
        if (line_idx + 1 < kl_line_count) {
            len = kl_line_starts[line_idx + 1] - start - 1;
        } else {
            len = log_size - start;
        }
        if (len > 80) len = 80;
        if (len < 0) len = 0;

        if (log_buf && len > 0 && start < log_size) {
            char tmp[96];
            if (len > 0) {
                int ti = 0;
                for (int j = 0; j < len && ti < 94; j++) {
                    char c = log_buf[start + j];
                    if (c >= 32) tmp[ti++] = c;
                    else if (c == '\t') tmp[ti++] = ' ';
                }
                tmp[ti] = 0;
                if (ti > 0)
                    gfx_draw_string_transparent(x + 4, ry, tmp, text);
            }
        }
    }
}

static void kl_key(int id, char key) {
    (void)id;
    if (key == 'q' || key == 'Q' || key == 27) {
        wm_close_window(kl_win_id);
        return;
    }
    int total = kl_line_count;
    int body_h = 400 - KL_HEADER_H - 4;
    int rows_vis = body_h / KL_ROW_H;
    if (rows_vis < 1) rows_vis = 1;

    if (KEY_MATCH((unsigned char)key, KEY_UP)) {
        if (kl_scroll > 0) kl_scroll--;
    } else if (KEY_MATCH((unsigned char)key, KEY_DOWN)) {
        int max_scroll = total - rows_vis;
        if (max_scroll < 0) max_scroll = 0;
        if (kl_scroll < max_scroll) kl_scroll++;
    } else if (KEY_MATCH((unsigned char)key, KEY_PAGE_UP)) {
        kl_scroll -= rows_vis;
        if (kl_scroll < 0) kl_scroll = 0;
    } else if (KEY_MATCH((unsigned char)key, KEY_PAGE_DOWN)) {
        kl_scroll += rows_vis;
        int max_scroll = total - rows_vis;
        if (max_scroll < 0) max_scroll = 0;
        if (kl_scroll > max_scroll) kl_scroll = max_scroll;
    }
}

static void kl_resize(int id, int w, int h) {
    (void)id; (void)w; (void)h;
}

void kernellog_app(void) {
    if (wm_get_window(kl_win_id)) { wm_bring_to_front(kl_win_id); return; }
    kl_scroll = 0;
    kl_build_lines();
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    kl_win_id = wm_open_window(
        (fw - 680) / 2,
        (fh - 420) / 2,
        680, 420,
        "Kernel Log",
        0xD29922,
        kl_render, kl_key, kl_resize
    );
    wm_set_mouse_handler(kl_win_id, NULL);
}

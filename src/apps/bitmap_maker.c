// ============================================================
//  BEDI OS — Bitmap Maker
//  32x32 pixel editor with palette, pen, eraser, clear, save
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "apps/save_dialog.h"
#include "filesystem/filesystem.h"
#include "libs/bmp.h"
#include "kernel/mem/kheap.h"
#include <stdint.h>
#include <string.h>

#define BM_GRID_SIZE   32
#define BM_TOOLBAR_H   34
#define BM_STATUSBAR_H 20

#define BM_BTN_PEN     1
#define BM_BTN_ERASER  2
#define BM_BTN_CLEAR   3
#define BM_BTN_SAVE    4

static int bm_win_id;
static uint8_t bm_pixels[BM_GRID_SIZE][BM_GRID_SIZE];
static int bm_current_color;
static int bm_current_tool; // 0=pen, 1=eraser
static int bm_dragging;

static const uint32_t bm_palette[16] = {
    0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00,
    0xFF00FF, 0x00FFFF, 0xFBBF24, 0x3FB950, 0x58A6FF, 0xBC8CFF,
    0xF85149, 0xF0883E, 0x8B949E, 0x6D7079
};

static const char* bm_basename(const char* path) {
    const char* p = path;
    const char* last = path;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    return last;
}

static void bm_do_save(const char* path) {
    const char* fn = bm_basename(path);
    bmp_image_t tmp;
    tmp.width = BM_GRID_SIZE;
    tmp.height = BM_GRID_SIZE;
    tmp.bpp = 24;
    tmp.palette_size = 0;
    tmp.pixels = (uint8_t*)kmalloc((size_t)(BM_GRID_SIZE * BM_GRID_SIZE * 3));
    if (!tmp.pixels) return;
    for (int r = 0; r < BM_GRID_SIZE; r++) {
        for (int c = 0; c < BM_GRID_SIZE; c++) {
            uint8_t idx = bm_pixels[r][c] & 15;
            uint32_t col = bm_palette[idx];
            uint8_t* dst = tmp.pixels + (r * BM_GRID_SIZE + c) * 3;
            dst[0] = (col >> 16) & 0xFF; // R
            dst[1] = (col >> 8) & 0xFF;  // G
            dst[2] = col & 0xFF;         // B
        }
    }
    uint8_t* data = 0;
    int len = 0;
    if (bmp_encode(&tmp, &data, &len)) {
        fs_delete(fn);
        int fd = fs_create(fn);
        if (fd >= 0) {
            fs_write(fd, (const char*)data, len);
            fs_close(fd);
        }
        kfree(data);
    }
    kfree(tmp.pixels);
}

static void bm_draw_grid(int x, int y, int cell) {
    gfx_fill_rect(x, y, cell * BM_GRID_SIZE, cell * BM_GRID_SIZE, 0x1A1D23);
    gfx_draw_rect_outline(x, y, cell * BM_GRID_SIZE, cell * BM_GRID_SIZE, 1, 0x30363D);
    for (int r = 0; r < BM_GRID_SIZE; r++) {
        for (int c = 0; c < BM_GRID_SIZE; c++) {
            int px = x + c * cell;
            int py = y + r * cell;
            uint32_t col = bm_palette[bm_pixels[r][c] & 15];
            gfx_fill_rect(px, py, cell - 1, cell - 1, col);
        }
    }
}

static void bm_draw_palette(int x, int y) {
    int sw = 18;
    gfx_draw_string_transparent(x, y - 14, "Palette", 0x6D7079);
    for (int i = 0; i < 16; i++) {
        int rx = x + (i % 8) * (sw + 4);
        int ry = y + (i / 8) * (sw + 4);
        uint32_t col = bm_palette[i];
        gfx_fill_rect(rx, ry, sw, sw, col);
        gfx_draw_rect_outline(rx, ry, sw, sw, 1, 0x30363D);
        if (i == bm_current_color) {
            gfx_draw_rect_outline(rx - 1, ry - 1, sw + 2, sw + 2, 1, 0xFFFFFF);
        }
    }
}

static void bm_render(int id, int x, int y, int win_w, int win_h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gfx_fill_rect(x, y, win_w, win_h, 0x0D0E12);

    int grid_x = x + 12;
    int grid_y = y + 28; // below toolbar buttons
    int avail_w = win_w - 160 - 20;
    int cell = (avail_w / BM_GRID_SIZE);
    if (cell < 2) cell = 2;
    if (cell > 16) cell = 16;

    bm_draw_grid(grid_x, grid_y, cell);

    int pal_x = x + win_w - 140;
    int pal_y = grid_y;
    if (pal_x < grid_x + 10) pal_x = grid_x + 10;
    bm_draw_palette(pal_x, pal_y);

    gfx_fill_rect(x, y + win_h - BM_STATUSBAR_H, win_w, BM_STATUSBAR_H, 0x15171D);
    gfx_draw_hline(x, y + win_h - BM_STATUSBAR_H, win_w, 0x30363D);
    const char* mode = bm_current_tool ? "Eraser" : "Pen";
    gfx_draw_string_transparent(x + 10, y + win_h - BM_STATUSBAR_H + 5,
                                "Bitmap Maker - 32x32", 0x6D7079);
}

static void bm_on_key(int id, char key) {
    (void)id;
    if (key == 27) {
        wm_close_window(bm_win_id);
    }
}

static void bm_on_mouse(int id, int mx, int my, int mb) {
    (void)id;
    wm_window_t* win = wm_get_window(bm_win_id);
    if (!win) return;

    int cell = (win->w - 160 - 20) / BM_GRID_SIZE;
    if (cell < 2) cell = 2;
    if (cell > 16) cell = 16;

    int grid_x = 12;
    int grid_y = 28;
    int pal_x = win->w - 140;
    int pal_y = grid_y;
    if (pal_x < grid_x + 10) pal_x = grid_x + 10;
    int sw = 18;

    // Drawing on grid
    if (bm_dragging && (mb & 1)) {
        int rx = mx - grid_x;
        int ry = my - grid_y;
        if (rx >= 0 && ry >= 0) {
            int c = rx / cell;
            int r = ry / cell;
            if (r >= 0 && r < BM_GRID_SIZE && c >= 0 && c < BM_GRID_SIZE) {
                if (bm_current_tool == 0) {
                    bm_pixels[r][c] = bm_current_color;
                } else {
                    bm_pixels[r][c] = 0;
                }
            }
        }
        return;
    }
    if (!(mb & 1)) {
        bm_dragging = 0;
    }

    // Click handling
    if (mb & 1 && !bm_dragging) {
        int rx = mx - grid_x;
        int ry = my - grid_y;
        if (rx >= 0 && ry >= 0) {
            int c = rx / cell;
            int r = ry / cell;
            if (r >= 0 && r < BM_GRID_SIZE && c >= 0 && c < BM_GRID_SIZE) {
                bm_dragging = 1;
                if (bm_current_tool == 0) {
                    bm_pixels[r][c] = bm_current_color;
                } else {
                    bm_pixels[r][c] = 0;
                }
                return;
            }
        }
        for (int i = 0; i < 16; i++) {
            int px = pal_x + (i % 8) * (sw + 4);
            int py = pal_y + (i / 8) * (sw + 4);
            if (mx >= px && mx < px + sw && my >= py && my < py + sw) {
                bm_current_color = i;
                bm_current_tool = 0;
                return;
            }
        }
    }
}

static void bm_btn_pen(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    bm_current_tool = 0;
}
static void bm_btn_eraser(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    bm_current_tool = 1;
}
static void bm_btn_clear(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    memset(bm_pixels, 0, sizeof(bm_pixels));
}
static void bm_btn_save(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    save_dialog_open("bitmap.bmp", bm_do_save);
}

void bitmap_maker_app(void) {
    memset(bm_pixels, 0, sizeof(bm_pixels));
    bm_current_color = 1;
    bm_current_tool = 0;
    bm_dragging = 0;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 640, win_h = 420;
    if (win_w > (int)fw - 40) win_w = (int)fw - 40;
    if (win_h > (int)fh - 80) win_h = (int)fh - 80;
    if (win_w < 320) win_w = 320;
    if (win_h < 240) win_h = 240;

    int mx = (int)fw / 2;
    int my = (int)fh / 2;

    bm_win_id = wm_open_window(mx - win_w / 2, my - win_h / 2, win_w, win_h,
                   "Bitmap Maker", 0x4D5059, bm_render, bm_on_key, 0);
    if (bm_win_id >= 0) {
        wm_set_mouse_handler(bm_win_id, bm_on_mouse);
        wm_clear_buttons(bm_win_id);
        int bx = 6, by = 4, bw = 52, bh = 22, gap = 6;
        wm_add_button(bm_win_id, BM_BTN_PEN, bx, by, bw, bh, "Pen", 0x21262D, 0xE6EDF3, bm_btn_pen);
        bx += bw + gap;
        wm_add_button(bm_win_id, BM_BTN_ERASER, bx, by, bw, bh, "Eraser", 0x21262D, 0xE6EDF3, bm_btn_eraser);
        bx += bw + gap;
        wm_add_button(bm_win_id, BM_BTN_CLEAR, bx, by, bw, bh, "Clear", 0x21262D, 0xE6EDF3, bm_btn_clear);
        bx += bw + gap;
        wm_add_button(bm_win_id, BM_BTN_SAVE, bx, by, bw, bh, "Save", 0x21262D, 0xE6EDF3, bm_btn_save);
    }
}

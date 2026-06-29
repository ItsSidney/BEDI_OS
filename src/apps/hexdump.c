// ============================================================
//  BEDI OS — Hexdump
//  Modern hex viewer with offset, hex, and ASCII columns
// ============================================================
#include <stdint.h>
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "filesystem/filesystem.h"
#include "kernel/mem/kheap.h"
#include "kernel/time/timer.h"
#include "apps/load_dialog.h"
#include "apps/hexdump.h"
#include "string.h"

void hexdump_load_dialog_cb(const char* path);

#define HD_BYTES_PER_ROW  16
#define HD_CHAR_W         8
#define HD_LINE_H         16
#define HD_TOOLBAR_H      28
#define HD_STATUSBAR_H    22
#define HD_GUTTER_W       10
#define HD_MAX_FILE       (4UL * 1024 * 1024)  // 4 MiB max

static uint8_t* hd_data = 0;
static int hd_len = 0;
static int hd_capacity = 0;
static int hd_scroll_offset = 0;
static int hd_win_id = -1;
static char hd_filename[64];
static int hd_prev_mbtn = 0;
static int hd_drag_vsb = 0;

// ── Hex helpers ──────────────────────────────────────────────
static void hd_write_hex_byte(char* buf, int off, uint8_t v) {
    const char* hex = "0123456789ABCDEF";
    buf[off]   = hex[(v >> 4) & 0xF];
    buf[off+1] = hex[v & 0xF];
}
static void hd_write_hex32(char* buf, uint32_t v) {
    hd_write_hex_byte(buf, 0, (v >> 24) & 0xFF);
    hd_write_hex_byte(buf, 2, (v >> 16) & 0xFF);
    hd_write_hex_byte(buf, 4, (v >> 8) & 0xFF);
    hd_write_hex_byte(buf, 6, v & 0xFF);
}

// ── Render ───────────────────────────────────────────────────
static void hd_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;

    // ── Background ───────────────────────────────────────────
    gfx_fill_rect(x, y, w, h, 0x0D1117);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int mrel = !(mbtn & 1) && (hd_prev_mbtn & 1);
    hd_prev_mbtn = mbtn;

    // ── Toolbar ──────────────────────────────────────────────
    gfx_fill_rect(x, y, w, HD_TOOLBAR_H, 0x161B22);
    gfx_draw_hline(x, y + HD_TOOLBAR_H - 1, w, 0x30363D);
    gfx_draw_string_transparent(x + 10, y + 5, "Hex Viewer", 0x8B949E);

    // Open button
    int ob_x = x + w - 90, ob_y = y + 2, ob_w = 70, ob_h = HD_TOOLBAR_H - 4;
    int ob_hover = (mx >= ob_x && mx < ob_x + ob_w && my >= ob_y && my < ob_y + ob_h);
    gfx_fill_rect(ob_x, ob_y, ob_w, ob_h, ob_hover ? 0x1A437A : 0x0D1117);
    gfx_draw_rect_outline(ob_x, ob_y, ob_w, ob_h, 1, ob_hover ? 0x1F6FEB : 0x21262D);
    gfx_draw_string_transparent(ob_x + 16, ob_y + 5, "Open", ob_hover ? 0xFFFFFF : 0xC9D1D9);
    if ((mbtn & 1) && !(hd_prev_mbtn & 1) && ob_hover) {
        load_dialog_open(hexdump_load_dialog_cb);
    }

    // ── Content area ─────────────────────────────────────────
    int cx = x + HD_GUTTER_W;
    int cy = y + HD_TOOLBAR_H + 4;
    int cw = w - HD_GUTTER_W - 12;
    int ch = h - HD_TOOLBAR_H - HD_STATUSBAR_H - 8;
    int vis_rows = ch / HD_LINE_H;
    if (vis_rows < 1) vis_rows = 1;

    // Column positions
    int off_col_w = 9 * HD_CHAR_W + 4;    // "00000000  "
    int hex_col_x = cx + off_col_w;
    int hex_col_w = (HD_BYTES_PER_ROW * 3) * HD_CHAR_W;  // "FF FF FF ... "
    int ascii_col_x = hex_col_x + hex_col_w + 8;
    int ascii_col_w = HD_BYTES_PER_ROW * HD_CHAR_W;

    // ── Column headers ──────────────────────────────────────
    uint32_t hdr_clr = 0x484F58;
    gfx_draw_string_transparent(cx, cy - 2, "Offset", hdr_clr);
    gfx_draw_string_transparent(hex_col_x, cy - 2, "Bytes", hdr_clr);
    gfx_draw_string_transparent(ascii_col_x, cy - 2, "ASCII", hdr_clr);
    gfx_draw_hline(cx, cy + 11, ascii_col_x + ascii_col_w - cx, 0x21262D);

    cy += 14;

    // ── Data rows ────────────────────────────────────────────
    int total_rows = (hd_len + HD_BYTES_PER_ROW - 1) / HD_BYTES_PER_ROW;
    if (total_rows < 1) total_rows = 1;
    int start_row = hd_scroll_offset;
    int max_scroll = total_rows - vis_rows;
    if (max_scroll < 0) max_scroll = 0;

    char line_buf[256];
    for (int r = 0; r < vis_rows && start_row + r < total_rows; r++) {
        int row_y = cy + r * HD_LINE_H;
        int row_off = (start_row + r) * HD_BYTES_PER_ROW;

        // Alternate row bg
        if ((start_row + r) % 2 == 1)
            gfx_fill_rect(cx, row_y, ascii_col_x + ascii_col_w - cx, HD_LINE_H, 0x0F141D);

        // Offset column
        hd_write_hex32(line_buf, row_off);
        line_buf[8] = 0;
        gfx_draw_string_transparent(cx, row_y + 2, line_buf, 0x58A6FF);

        // Hex bytes column
        int bi = 0;
        for (int c = 0; c < HD_BYTES_PER_ROW; c++) {
            int byte_idx = row_off + c;
            if (byte_idx < hd_len) {
                hd_write_hex_byte(line_buf, bi, hd_data[byte_idx]);
                bi += 2;
                line_buf[bi++] = ' ';
            } else {
                line_buf[bi++] = ' ';
                line_buf[bi++] = ' ';
                line_buf[bi++] = ' ';
            }
        }
        line_buf[bi] = 0;
        gfx_draw_string_transparent(hex_col_x, row_y + 2, line_buf, 0xE6EDF3);

        // ASCII column
        bi = 0;
        for (int c = 0; c < HD_BYTES_PER_ROW; c++) {
            int byte_idx = row_off + c;
            if (byte_idx < hd_len) {
                uint8_t b = hd_data[byte_idx];
                line_buf[bi++] = (b >= 32 && b < 127) ? b : '.';
            } else {
                line_buf[bi++] = ' ';
            }
        }
        line_buf[bi] = 0;
        gfx_draw_string_transparent(ascii_col_x, row_y + 2, line_buf, 0xC9D1D9);
    }

    // ── Vertical scrollbar ───────────────────────────────────
    int vsb_x = x + w - 10;
    int vsb_y = cy;
    int vsb_h = ch - 14;
    gfx_fill_rect(vsb_x, vsb_y, 10, vsb_h, 0x0A0E14);
    gfx_draw_vline(vsb_x, vsb_y, vsb_h, 0x21262D);
    gfx_draw_vline(vsb_x + 9, vsb_y, vsb_h, 0x21262D);
    int vsb_thumb_h = (vsb_h * vis_rows) / total_rows;
    if (vsb_thumb_h < 16) vsb_thumb_h = 16;
    int vsb_range = vsb_h - vsb_thumb_h;
    int vsb_thumb_y = vsb_y + (max_scroll > 0 ? (hd_scroll_offset * vsb_range) / max_scroll : 0);
    gfx_fill_rect(vsb_x + 1, vsb_thumb_y, 8, vsb_thumb_h, 0x30363D);

    // Scrollbar drag
    if (hd_drag_vsb) {
        if (mbtn & 1) {
            int my_local = mouse_get_y();
            int rel_y = my_local - vsb_y - vsb_thumb_h / 2;
            if (vsb_range > 0) {
                int new_off = (rel_y * max_scroll) / vsb_range;
                if (new_off < 0) new_off = 0;
                if (new_off > max_scroll) new_off = max_scroll;
                hd_scroll_offset = new_off;
            }
        } else {
            hd_drag_vsb = 0;
        }
    }

    // Scrollbar click (not drag)
    if (mrel) hd_drag_vsb = 0;
    if ((mbtn & 1) && !hd_drag_vsb) {
        int my_local = mouse_get_y();
        if (my_local >= vsb_y && my_local < vsb_y + vsb_h &&
            mouse_get_x() >= vsb_x && mouse_get_x() < vsb_x + 10) {
            // Check if on thumb
            if (my_local >= vsb_thumb_y && my_local < vsb_thumb_y + vsb_thumb_h) {
                hd_drag_vsb = 1;
            } else {
                int rel_y = my_local - vsb_y - vsb_thumb_h / 2;
                if (vsb_range > 0) {
                    int new_off = (rel_y * max_scroll) / vsb_range;
                    if (new_off < 0) new_off = 0;
                    if (new_off > max_scroll) new_off = max_scroll;
                    hd_scroll_offset = new_off;
                }
            }
        }
    }

    // ── Status bar ───────────────────────────────────────────
    int sb_y = y + h - HD_STATUSBAR_H;
    gfx_fill_rect(x, sb_y, w, HD_STATUSBAR_H, 0x161B22);
    gfx_draw_hline(x, sb_y, w, 0x30363D);

    char sb[128]; int si = 0;
    if (hd_len > 0) {
        const char* fn = hd_filename;
        while (*fn && si < 40) sb[si++] = *fn++;
        sb[si++] = ' ';
        sb[si++] = '-';
        sb[si++] = ' ';
        // File size in KB
        if (hd_len < 1024) {
            hd_write_hex32(sb + si, (uint32_t)hd_len);
            si += 8;
        } else {
            int kb = hd_len / 1024;
            int mb = kb / 1024;
            if (mb > 0) {
                if (mb >= 10) { sb[si++] = '0' + mb / 10; }
                sb[si++] = '0' + mb % 10;
                sb[si++] = 'M'; sb[si++] = 'B';
            } else {
                if (kb >= 100) { sb[si++] = '0' + kb / 100; }
                if (kb >= 10) { sb[si++] = '0' + (kb / 10) % 10; }
                sb[si++] = '0' + kb % 10;
                sb[si++] = 'K'; sb[si++] = 'B';
            }
        }
        sb[si++] = ' ';
        sb[si++] = '(';
        int ti = 0; char tbuf[16];
        itoa(hd_len, tbuf);
        while (tbuf[ti]) sb[si++] = tbuf[ti++];
        sb[si++] = ' ';
        sb[si++] = 'b'; sb[si++] = 'y'; sb[si++] = 't'; sb[si++] = 'e'; sb[si++] = 's';
        sb[si++] = ')';
    } else {
        const char* txt = "No file loaded";
        while (*txt && si < 60) sb[si++] = *txt++;
    }
    sb[si] = 0;
    gfx_draw_string_transparent(x + 10, sb_y + 3, sb, 0x6D7079);
}

// ── Keyboard ─────────────────────────────────────────────────
static void hd_on_key(int id, char key_in) {
    (void)id;
    unsigned char k = (unsigned char)key_in;
    if (hd_len <= 0) { if (k == 27) wm_close_window(hd_win_id); return; }

    int total_rows = (hd_len + HD_BYTES_PER_ROW - 1) / HD_BYTES_PER_ROW;
    wm_window_t* win = wm_get_window(hd_win_id);
    int ch = win ? (win->h - HD_TOOLBAR_H - HD_STATUSBAR_H - 8) : 400;
    int vis_rows = ch / HD_LINE_H;
    if (vis_rows < 1) vis_rows = 1;
    int max_scroll = total_rows - vis_rows;
    if (max_scroll < 0) max_scroll = 0;

    if (k == 27) {
        if (hd_data) { kfree(hd_data); hd_data = 0; hd_len = 0; }
        wm_close_window(hd_win_id);
        return;
    }
    if (k == KEY_DOWN || k == 129) {
        if (hd_scroll_offset < max_scroll) hd_scroll_offset++;
        else if (total_rows > vis_rows) hd_scroll_offset = max_scroll;
        return;
    }
    if (k == KEY_UP || k == 128) {
        if (hd_scroll_offset > 0) hd_scroll_offset--;
        return;
    }
    if (k == KEY_PAGE_DOWN || k == 134) {
        hd_scroll_offset += vis_rows;
        if (hd_scroll_offset > max_scroll) hd_scroll_offset = max_scroll;
        return;
    }
    if (k == KEY_PAGE_UP || k == 133) {
        hd_scroll_offset -= vis_rows;
        if (hd_scroll_offset < 0) hd_scroll_offset = 0;
        return;
    }
    if (k == KEY_HOME || k == 137) {
        hd_scroll_offset = 0;
        return;
    }
    if (k == KEY_END || k == 138) {
        hd_scroll_offset = max_scroll;
        return;
    }
    // Ctrl+O
    if (k == 15) {
        load_dialog_open(hexdump_load_dialog_cb);
        return;
    }
}

// ── Mouse ────────────────────────────────────────────────────
static void hd_on_mouse(int id, int mx, int my, int mb) {
    (void)id;
    if (mb & 1) {
        // Mouse wheel (if supported via buttons)
        // Already handled by scrollbar in render
    }
}

// ── API ──────────────────────────────────────────────────────
void hexdump_load_dialog_cb(const char* path) {
    hexdump_open(path);
}

void hexdump_open(const char* filename) {
    if (hd_win_id >= 0) {
        wm_close_window(hd_win_id);
        if (hd_data) { kfree(hd_data); hd_data = 0; }
        hd_len = 0;
    }
    hd_scroll_offset = 0;

    // Copy filename
    int fi = 0;
    while (filename[fi] && fi < 63) { hd_filename[fi] = filename[fi]; fi++; }
    hd_filename[fi] = 0;

    // Open and read file
    int fd = fs_open(filename, 0);
    if (fd >= 0) {
        int size = 0;
        fs_get_node(fd, NULL, &size, NULL, NULL, NULL, NULL);
        if (size > 0 && size <= (int)HD_MAX_FILE) {
            hd_data = (uint8_t*)kmalloc(size);
            if (hd_data) {
                int rd = fs_read(fd, (char*)hd_data, size);
                if (rd == size) {
                    hd_len = size;
                    hd_capacity = size;
                } else {
                    kfree(hd_data); hd_data = 0;
                }
            }
        }
        fs_close(fd);
    }

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 680, win_h = 500;
    hd_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                  "Hex Viewer", 0x3FB950, hd_render, hd_on_key, 0);
    if (hd_win_id >= 0) {
        wm_set_mouse_handler(hd_win_id, hd_on_mouse);
    }
}

void hexdump_app(void) {
    load_dialog_open(hexdump_load_dialog_cb);
}

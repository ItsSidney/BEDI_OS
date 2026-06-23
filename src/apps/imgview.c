#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "filesystem/filesystem.h"
#include "libs/bmp.h"
#include "kernel/mem/kheap.h"

#define MAX_IMG 64
#define LIST_W 180

static int iv_win_id;

static bmp_image_t cur_img;
static int img_loaded;
static char img_name[64];
static int prev_mbtn;

// ── Zoom / Pan state ──────────────────────────────────────────
#define ZOOM_STEPS 9
static const float zoom_factors[ZOOM_STEPS] = {
    0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f
};
static int zoom_idx;        // index into zoom_factors
static float cur_zoom;
static int fit_view;        // 1 = auto-fit, 0 = manual zoom
static int pan_x, pan_y;    // pan offset in screen pixels
static int dragging;
static int drag_prev_x, drag_prev_y;

// ── File list ──────────────────────────────────────────────────
static int bmp_count;
static char bmp_names[MAX_IMG][64];
static int sel_idx;

static void scan_files(void) {
    bmp_count = 0;
    for (int i = 0; i < 256 && bmp_count < MAX_IMG; i++) {
        char name[64]; int sz, typ, par; uint8_t flg; uint32_t mt;
        if (fs_get_node(i, name, &sz, &typ, &par, &flg, &mt) == 0 && typ == 0) {
            int len = 0; while (name[len]) len++;
            if (len > 4 && name[len-4] == '.' &&
                (name[len-3] == 'b' || name[len-3] == 'B') &&
                (name[len-2] == 'm' || name[len-2] == 'M') &&
                (name[len-1] == 'p' || name[len-1] == 'P')) {
                int j = 0; while (name[j] && j < 63) { bmp_names[bmp_count][j] = name[j]; j++; }
                bmp_names[bmp_count][j] = 0;
                bmp_count++;
            }
        }
    }
}

// ── Load file ──────────────────────────────────────────────────
static int load_file(const char* name) {
    bmp_free(&cur_img);
    img_loaded = 0;
    int fd = fs_open(name, 0);
    if (fd < 0) return 0;
    int size = 0;
    for (int i = 0; i < 256; i++) {
        char buf[64]; int sz, typ, par; uint8_t flg; uint32_t mt;
        if (fs_get_node(i, buf, &sz, &typ, &par, &flg, &mt) == 0) {
            int match = 1;
            for (int j = 0; name[j] && buf[j]; j++)
                if (name[j] != buf[j]) { match = 0; break; }
            if (match && name[0] && buf[0]) { size = sz; break; }
        }
    }
    if (size <= 0) { fs_close(fd); return 0; }
    uint8_t* buf = kmalloc(size);
    if (!buf) { fs_close(fd); return 0; }
    int rd = fs_read(fd, (char*)buf, size);
    fs_close(fd);
    if (rd != size) { kfree(buf); return 0; }
    if (!bmp_decode(buf, size, &cur_img)) { kfree(buf); return 0; }
    kfree(buf);
    img_loaded = 1;
    int j = 0; while (name[j] && j < 63) { img_name[j] = name[j]; j++; } img_name[j] = 0;

    fit_view = 1;
    zoom_idx = 4;
    cur_zoom = 1.0f;
    pan_x = 0; pan_y = 0;
    return 1;
}

// ── Reset zoom / fit ──────────────────────────────────────────
static void reset_view(void) {
    fit_view = 1;
    pan_x = 0; pan_y = 0;
    zoom_idx = 4;
    cur_zoom = 1.0f;
}

// ── Compute display dimensions and offset for current zoom ────
static void compute_view(int view_w, int view_h,
                         int* out_dw, int* out_dh,
                         int* out_dx, int* out_dy,
                         float* out_scale) {
    float scale;
    if (fit_view || !img_loaded) {
        float sx = (float)(view_w - 16) / cur_img.width;
        float sy = (float)(view_h - 16) / cur_img.height;
        scale = sx < sy ? sx : sy;
        if (scale > 1.0f) scale = 1.0f;
    } else {
        scale = cur_zoom;
    }
    *out_scale = scale;
    int dw = (int)(cur_img.width * scale);
    int dh = (int)(cur_img.height * scale);
    *out_dw = dw; *out_dh = dh;
    int dx = (view_w - dw) / 2 + pan_x;
    int dy = (view_h - dh) / 2 + pan_y;
    // Clamp pan so image doesn't go off edge
    if (dw < view_w) {
        if (dx < 0) dx = 0;
        if (dx + dw > view_w) dx = view_w - dw;
    }
    if (dh < view_h) {
        if (dy < 0) dy = 0;
        if (dy + dh > view_h) dy = view_h - dh;
    }
    *out_dx = dx; *out_dy = dy;
}

// ── Render callback ──────────────────────────────────────────
static void iv_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0E12);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int mclick = (mbtn & 1) && !(prev_mbtn & 1);
    int mrel = !(mbtn & 1) && (prev_mbtn & 1);

    int view_x = x + LIST_W;
    int view_w = w - LIST_W;
    int view_h = h - 16;

    // ── File list panel ──
    int fy = y + 4;
    for (int i = 0; i < bmp_count; i++) {
        int iy = fy + i * 20;
        if (iy + 20 > y + h) break;
        int hover = (mx >= x + 4 && mx < x + LIST_W - 4 && my >= iy && my < iy + 18);
        if (mclick && hover) {
            sel_idx = i;
            load_file(bmp_names[i]);
            mclick = 0;
        }
        uint32_t bg = (i == sel_idx) ? 0x262830 : (hover ? 0x1D1F26 : 0x0D0E12);
        gfx_fill_rect(x + 4, iy, LIST_W - 8, 18, bg);
        gfx_draw_string_transparent(x + 8, iy + 2, bmp_names[i], (i == sel_idx) ? 0xE4E6EA : 0x94979F);
    }
    gfx_draw_line(x + LIST_W, y, x + LIST_W, y + h, 0x262830);

    // ── Image view area ──
    if (img_loaded && cur_img.pixels) {
        // Pan with mouse drag in view area
        int in_view = (mx >= view_x && mx < view_x + view_w && my >= y && my < y + h);
        if (mclick && in_view && !fit_view) {
            dragging = 1;
            drag_prev_x = mx; drag_prev_y = my;
        }
        if (mrel) dragging = 0;
        if (dragging && (mbtn & 1)) {
            pan_x += mx - drag_prev_x;
            pan_y += my - drag_prev_y;
            drag_prev_x = mx; drag_prev_y = my;
        }

        int dw, dh, dx, dy; float scale;
        compute_view(view_w, view_h, &dw, &dh, &dx, &dy, &scale);
        dx += view_x;

        // Draw outline around image
        gfx_draw_rect_outline(dx - 1, dy - 1, dw + 2, dh + 2, 1, 0x383B44);

        // Checkerboard background for transparency indication on small images
        if (dw < view_w && dh < view_h) {
            for (int ck_y = 0; ck_y < dh; ck_y += 8) {
                for (int ck_x = 0; ck_x < dw; ck_x += 8) {
                    int c = ((ck_x / 8 + ck_y / 8) & 1) ? 0x1D1F26 : 0x15171D;
                    int cw = (ck_x + 8 > dw) ? (dw - ck_x) : 8;
                    int ch = (ck_y + 8 > dh) ? (dh - ck_y) : 8;
                    gfx_fill_rect(dx + ck_x, dy + ck_y, cw, ch, c);
                }
            }
        }

        gfx_draw_rgb_bitmap_scaled(dx, dy, dw, dh, cur_img.pixels, cur_img.width, cur_img.height);

        // Status bar
        char info[160];
        int si = 0;
        const char* fn = img_name;
        while (*fn) info[si++] = *fn++;
        info[si++] = ' '; info[si++] = '(';
        if (cur_img.width >= 100) info[si++] = '0' + cur_img.width / 100;
        if (cur_img.width >= 10) info[si++] = '0' + (cur_img.width / 10) % 10;
        info[si++] = '0' + cur_img.width % 10;
        info[si++] = 'x';
        if (cur_img.height >= 100) info[si++] = '0' + cur_img.height / 100;
        if (cur_img.height >= 10) info[si++] = '0' + (cur_img.height / 10) % 10;
        info[si++] = '0' + cur_img.height % 10;
        info[si++] = ')';
        int zoom_pct = (int)(scale * 100.0f + 0.5f);
        info[si++] = ' ';
        if (zoom_pct >= 100) info[si++] = '0' + zoom_pct / 100;
        if (zoom_pct >= 10) info[si++] = '0' + (zoom_pct / 10) % 10;
        info[si++] = '0' + zoom_pct % 10;
        info[si++] = '%';
        if (fit_view) { info[si++] = ' '; info[si++] = '('; info[si++] = 'F'; info[si++] = 'i'; info[si++] = 't'; info[si++] = ')'; }
        info[si] = 0;
        gfx_draw_string_transparent(view_x + 4, y + h - 16, info, 0x6D7079);
    } else if (bmp_count == 0) {
        gfx_draw_string_transparent(view_x + (view_w - 120) / 2, y + h / 2 - 8, "No BMP files found", 0x6D7079);
    } else {
        gfx_draw_string_transparent(view_x + (view_w - 120) / 2, y + h / 2 - 8, "Select a BMP file", 0x6D7079);
    }

    prev_mbtn = mbtn;
}

// ── Keyboard callback ─────────────────────────────────────────
static void iv_on_key(int id, char key) {
    (void)id;
    if (KEY_MATCH(key, 'r') || KEY_MATCH(key, 'R')) {
        scan_files();
        return;
    }
    if (!img_loaded) return;

    // Zoom in
    if (KEY_MATCH(key, '+') || KEY_MATCH(key, '=')) {
        if (fit_view) fit_view = 0;
        if (zoom_idx < ZOOM_STEPS - 1) zoom_idx++;
        cur_zoom = zoom_factors[zoom_idx];
        return;
    }
    // Zoom out
    if (KEY_MATCH(key, '-') || KEY_MATCH(key, '_')) {
        if (fit_view) fit_view = 0;
        if (zoom_idx > 0) zoom_idx--;
        cur_zoom = zoom_factors[zoom_idx];
        return;
    }
    // Fit to window
    if (KEY_MATCH(key, '0')) {
        fit_view = 1;
        pan_x = 0; pan_y = 0;
        return;
    }
    // Pan with arrow keys (only when not in fit view)
    if (!fit_view) {
        int step = 20;
        if (KEY_MATCH(key, KEY_LEFT) || KEY_MATCH(key, 'a') || KEY_MATCH(key, 'A')) pan_x += step;
        if (KEY_MATCH(key, KEY_RIGHT) || KEY_MATCH(key, 'd') || KEY_MATCH(key, 'D')) pan_x -= step;
        if (KEY_MATCH(key, KEY_UP) || KEY_MATCH(key, 'w') || KEY_MATCH(key, 'W')) pan_y += step;
        if (KEY_MATCH(key, KEY_DOWN) || KEY_MATCH(key, 's') || KEY_MATCH(key, 'S')) pan_y -= step;
    }
}

// ── Entry points ──────────────────────────────────────────────
static void open_impl(const char* name) {
    prev_mbtn = 0; sel_idx = -1; dragging = 0;
    cur_img.pixels = 0; img_loaded = 0;
    scan_files();
    reset_view();

    // Find the file in the list and select/load it
    if (name && name[0]) {
        for (int i = 0; i < bmp_count; i++) {
            int match = 1;
            for (int j = 0; name[j] && bmp_names[i][j]; j++)
                if (name[j] != bmp_names[i][j]) { match = 0; break; }
            if (match) {
                sel_idx = i;
                load_file(bmp_names[i]);
                break;
            }
        }
    }

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 700, win_h = 450;
    iv_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h + WM_TITLEBAR_H,
                   "Image Viewer", 0x4D5059, iv_render, iv_on_key, 0);
}

void imgview_app(void) {
    open_impl(0);
}

void imgview_open(const char* name) {
    open_impl(name);
}

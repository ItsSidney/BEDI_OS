// ============================================================
//  BEDI OS — Image Viewer / Converter
//  Modern BMP viewer + export tool
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "filesystem/filesystem.h"
#include "apps/save_dialog.h"
#include "libs/bmp.h"
#include "kernel/mem/kheap.h"
#include <stdint.h>
#include <string.h>

#define IV_MAX_IMGS   128
#define IV_LIST_W     200
#define IV_THUMB_H    64
#define IV_THUMB_GAP  6
#define IV_TOOLBAR_H  36
#define IV_STATUSBAR_H 24

// ── Image entry ──────────────────────────────────────────────
typedef struct {
    char  name[64];
    int   size;
    uint32_t mod_time;
    int   type; // 0 = bmp
} iv_img_t;

// ── Zoom / Pan state ─────────────────────────────────────────
#define ZOOM_STEPS 11
static const float zoom_factors[ZOOM_STEPS] = {
    0.125f, 0.25f, 0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f, 8.0f
};
static int zoom_idx;
static float cur_zoom;
static int fit_view;
static int pan_x, pan_y;
static int dragging;
static int drag_prev_x, drag_prev_y;

// ── Image list / state ───────────────────────────────────────
static int iv_win_id;
static bmp_image_t cur_img;
static int img_loaded;
static char img_name[64];
static int prev_mbtn;
static iv_img_t iv_images[IV_MAX_IMGS];
static int iv_img_count;
static int iv_sel_idx;
static int iv_mouse_down;
static int iv_rotation; // 0, 90, 180, 270
static int iv_dir_changed;

// ── Rotation buffer ──────────────────────────────────────────
static uint8_t* iv_rot_buf = 0;
static int iv_disp_w = 0, iv_disp_h = 0;
static int iv_prev_rotation = -1;

// ── Helper: basename ─────────────────────────────────────────
static const char* iv_basename(const char* path) {
    const char* p = path;
    const char* last = path;
    while (*p) { if (*p == '/') last = p + 1; p++; }
    return last;
}

// ── Helper: check image extension ────────────────────────────
static int is_image_ext(const char* name) {
    int len = 0; while (name[len]) len++;
    if (len < 5) return 0;
    const char* ext = name + len - 4;
    if (ext[0] != '.') return 0;
    return (ext[1] == 'b' || ext[1] == 'B') &&
           (ext[2] == 'm' || ext[2] == 'M') &&
           (ext[3] == 'p' || ext[3] == 'P');
}

// ── Scan current directory for images ────────────────────────
static void scan_files(void) {
    iv_img_count = 0;
    int curr_dir = fs_get_current_dir();
    if (curr_dir < 0) return;
    int count = fs_get_dir_count(curr_dir);
    if (count > IV_MAX_IMGS) count = IV_MAX_IMGS;
    for (int i = 0; i < count; i++) {
        char name[64]; int sz, tp; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(curr_dir, i, name, &sz, &tp, &fl, &mt) == 0) {
            if (tp == 0 && is_image_ext(name)) {
                iv_img_t* e = &iv_images[iv_img_count];
                int j = 0;
                while (name[j] && j < 63) { e->name[j] = name[j]; j++; }
                e->name[j] = 0;
                e->size = sz; e->mod_time = mt; e->type = 0;
                iv_img_count++;
            }
        }
    }
    if (iv_sel_idx >= iv_img_count) iv_sel_idx = iv_img_count - 1;
}

// ── Load file ────────────────────────────────────────────────
static int load_file(const char* name) {
    bmp_free(&cur_img);
    if (iv_rot_buf) { kfree(iv_rot_buf); iv_rot_buf = 0; }
    iv_prev_rotation = -1;
    img_loaded = 0;
    int fd = fs_open(name, 0);
    if (fd < 0) return 0;
    int size = 0;
    int curr_dir = fs_get_current_dir();
    int count = fs_get_dir_count(curr_dir);
    for (int i = 0; i < count; i++) {
        char buf[64]; int sz, typ; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(curr_dir, i, buf, &sz, &typ, &fl, &mt) == 0) {
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
    iv_rotation = 0;
    return 1;
}

static void reset_view(void) {
    fit_view = 1;
    pan_x = 0; pan_y = 0;
    zoom_idx = 4;
    cur_zoom = 1.0f;
    iv_rotation = 0;
    if (iv_rot_buf) { kfree(iv_rot_buf); iv_rot_buf = 0; }
    iv_prev_rotation = -1;
}

// ── Build rotated pixel buffer when angle changes ────────────
static void iv_update_rotation(void) {
    if (iv_rot_buf) { kfree(iv_rot_buf); iv_rot_buf = 0; }
    if (!img_loaded || !cur_img.pixels) { iv_disp_w = 0; iv_disp_h = 0; return; }

    int angle = iv_rotation % 360;
    if (angle < 0) angle += 360;

    if (angle == 0) {
        iv_disp_w = cur_img.width;
        iv_disp_h = cur_img.height;
        return;
    }

    int src_w = cur_img.width, src_h = cur_img.height;
    int dst_w = (angle == 90 || angle == 270) ? src_h : src_w;
    int dst_h = (angle == 90 || angle == 270) ? src_w : src_h;
    iv_disp_w = dst_w;
    iv_disp_h = dst_h;

    iv_rot_buf = (uint8_t*)kmalloc((size_t)(dst_w * dst_h * 3));
    if (!iv_rot_buf) { iv_disp_w = src_w; iv_disp_h = src_h; return; }

    for (int y = 0; y < dst_h; y++) {
        for (int x = 0; x < dst_w; x++) {
            int sx, sy;
            if (angle == 90)      { sx = src_h - 1 - y; sy = x; }
            else if (angle == 180) { sx = src_w - 1 - x; sy = src_h - 1 - y; }
            else /* 270 */        { sx = y; sy = src_w - 1 - x; }
            int si = (sy * src_w + sx) * 3;
            int di = (y * dst_w + x) * 3;
            iv_rot_buf[di]   = cur_img.pixels[si];
            iv_rot_buf[di+1] = cur_img.pixels[si+1];
            iv_rot_buf[di+2] = cur_img.pixels[si+2];
        }
    }
}

// ── Compute display dims/offset ──────────────────────────────
static void compute_view(int view_w, int view_h,
                         int* out_dw, int* out_dh,
                         int* out_dx, int* out_dy,
                         float* out_scale) {
    int img_w = iv_disp_w ? iv_disp_w : cur_img.width;
    int img_h = iv_disp_h ? iv_disp_h : cur_img.height;
    float scale;
    if (fit_view || !img_loaded) {
        float sx = (float)(view_w - 24) / img_w;
        float sy = (float)(view_h - 24) / img_h;
        scale = sx < sy ? sx : sy;
        if (scale > 1.0f) scale = 1.0f;
    } else {
        scale = cur_zoom;
    }
    *out_scale = scale;
    int dw = (int)(img_w * scale);
    int dh = (int)(img_h * scale);
    *out_dw = dw; *out_dh = dh;
    int dx = (view_w - dw) / 2 + pan_x;
    int dy = (view_h - dh) / 2 + pan_y;
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

// ── Export callback ──────────────────────────────────────────
static void iv_export_cb(const char* path) {
    if (!img_loaded) return;
    bmp_image_t tmp;
    tmp.width = cur_img.width;
    tmp.height = cur_img.height;
    tmp.bpp = 24;
    tmp.palette_size = cur_img.palette_size;
    for (int i = 0; i < cur_img.palette_size; i++) tmp.palette[i] = cur_img.palette[i];
    tmp.pixels = (uint8_t*)kmalloc((size_t)(cur_img.width * cur_img.height * 3));
    if (!tmp.pixels) return;
    memcpy(tmp.pixels, cur_img.pixels, (size_t)(cur_img.width * cur_img.height * 3));
    uint8_t* data = 0;
    int len = 0;
    if (bmp_encode(&tmp, &data, &len)) {
        const char* fn = iv_basename(path);
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

// ── Render ───────────────────────────────────────────────────
static void iv_render(int id, int x, int y, int w, int h, int vx, int vy);
static void draw_tb_btn(int x, int y, int w, int h, const char* lbl, int active, int hover);

static void iv_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0E12);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();
    int mclick = (mbtn & 1) && !(prev_mbtn & 1);
    int mrel = !(mbtn & 1) && (prev_mbtn & 1);
    prev_mbtn = mbtn;

    char cwd[256];
    fs_pwd(cwd, 256);
    static char prev_cwd[256];
    static int first_frame = 1;
    if (!first_frame && strcmp(cwd, prev_cwd) != 0) {
        iv_dir_changed = 1;
        iv_sel_idx = 0;
    }
    first_frame = 0;
    int j = 0;
    while (cwd[j] && j < 255) { prev_cwd[j] = cwd[j]; j++; }
    prev_cwd[j] = 0;

    if (iv_dir_changed) {
        scan_files();
        bmp_free(&cur_img);
        img_loaded = 0;
        img_name[0] = 0;
        reset_view();
        iv_dir_changed = 0;
        if (iv_img_count > 0) {
            load_file(iv_images[0].name);
            iv_sel_idx = 0;
        }
    }

    int tb_y = y + 2;
    int view_x = x + IV_LIST_W;
    int view_w = w - IV_LIST_W;
    int view_h = h - IV_TOOLBAR_H - IV_STATUSBAR_H - 16;
    int view_y = y + IV_TOOLBAR_H + 4;

    // ── Toolbar ──────────────────────────────────────────────
    gfx_fill_rect(x, tb_y, w, IV_TOOLBAR_H, 0x15171D);
    gfx_draw_hline(x, tb_y + IV_TOOLBAR_H - 1, w, 0x262830);

    int btn_x = x + 8, btn_y = tb_y + 4, btn_w = 64, btn_h = 26, btn_gap = 4;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "Fit", fit_view, 0);
    btn_x += btn_w + btn_gap;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "Zoom +", zoom_idx >= ZOOM_STEPS - 1, 0);
    btn_x += btn_w + btn_gap;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "Zoom -", zoom_idx <= 0, 0);
    btn_x += btn_w + btn_gap;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "100%", !fit_view && zoom_idx == 4, 0);
    btn_x += btn_w + btn_gap;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "Rotate", 0, 0);
    btn_x += btn_w + btn_gap;
    draw_tb_btn(btn_x, btn_y, btn_w, btn_h, "Export", 0, 0);

    // Toolbar click
    if (mclick && my >= tb_y && my < tb_y + IV_TOOLBAR_H && mx >= x && mx < x + w) {
        int bx = x + 8;
        if (mx >= bx && mx < bx + btn_w) { reset_view(); mclick = 0; }
        bx += btn_w + btn_gap;
        if (mx >= bx && mx < bx + btn_w && zoom_idx < ZOOM_STEPS - 1) {
            fit_view = 0; zoom_idx++; cur_zoom = zoom_factors[zoom_idx]; mclick = 0;
        }
        bx += btn_w + btn_gap;
        if (mx >= bx && mx < bx + btn_w && zoom_idx > 0) {
            fit_view = 0; zoom_idx--; cur_zoom = zoom_factors[zoom_idx]; mclick = 0;
        }
        bx += btn_w + btn_gap;
        if (mx >= bx && mx < bx + btn_w) {
            fit_view = 0; zoom_idx = 4; cur_zoom = 1.0f; pan_x = 0; pan_y = 0; mclick = 0;
        }
        bx += btn_w + btn_gap;
        if (mx >= bx && mx < bx + btn_w && img_loaded) {
            iv_rotation = (iv_rotation + 90) % 360; mclick = 0;
        }
        bx += btn_w + btn_gap;
        if (mx >= bx && mx < bx + btn_w && img_loaded) {
            save_dialog_open(img_name, iv_export_cb);
            mclick = 0;
        }
    }

    // ── File list panel ──────────────────────────────────────
    int list_x = x + 2;
    int list_top = y + IV_TOOLBAR_H + 2;
    gfx_fill_rect(list_x, list_top, IV_LIST_W - 2, 20, 0x15171D);
    gfx_draw_string_transparent(list_x + 6, list_top + 3, "Current Dir", 0x6D7079);
    gfx_draw_hline(list_x, list_top + 19, IV_LIST_W - 4, 0x262830);

    int fy = list_top + 22;
    for (int i = 0; i < iv_img_count; i++) {
        int iy = fy + i * 22;
        if (iy + 22 > y + h - IV_STATUSBAR_H) break;
        int hover = (mx >= list_x + 4 && mx < list_x + IV_LIST_W - 8 && my >= iy && my < iy + 20);
        uint32_t bg = (i == iv_sel_idx) ? 0x2D4A6F : (hover ? 0x1D1F26 : 0x0D0E12);
        gfx_fill_rect(list_x + 4, iy, IV_LIST_W - 10, 20, bg);
        gfx_draw_string_transparent(list_x + 8, iy + 3, iv_images[i].name, (i == iv_sel_idx) ? 0xFFFFFF : 0x94979F);
        if (mclick && hover) {
            iv_sel_idx = i;
            load_file(iv_images[i].name);
            mclick = 0;
        }
    }
    gfx_draw_line(x + IV_LIST_W, list_top, x + IV_LIST_W, y + h, 0x262830);

    // ── Image view area ──────────────────────────────────────
    if (img_loaded && cur_img.pixels) {
        int in_view = (mx >= view_x && mx < view_x + view_w && my >= view_y && my < view_y + view_h);
        if (mclick && in_view) {
            if (!fit_view) {
                dragging = 1;
                drag_prev_x = mx; drag_prev_y = my;
            }
            iv_mouse_down = 1;
        }
        if (mrel) {
            dragging = 0; iv_mouse_down = 0;
            prev_mbtn = mbtn;
        }
        if (dragging && (mbtn & 1)) {
            pan_x += mx - drag_prev_x;
            pan_y += my - drag_prev_y;
            drag_prev_x = mx; drag_prev_y = my;
        }

        /* Rebuild rotation buffer if angle changed */
        if (iv_rotation != iv_prev_rotation) {
            iv_update_rotation();
            iv_prev_rotation = iv_rotation;
        }

        int dw, dh, dx, dy; float scale;
        compute_view(view_w, view_h, &dw, &dh, &dx, &dy, &scale);
        dx += view_x; dy += view_y;

        // Checkerboard for transparency
        if (dw < view_w - 4 && dh < view_h - 4) {
            for (int ck_y = 0; ck_y < dh; ck_y += 10) {
                for (int ck_x = 0; ck_x < dw; ck_x += 10) {
                    int c = ((ck_x / 10 + ck_y / 10) & 1) ? 0x1D1F26 : 0x15171D;
                    int cw = (ck_x + 10 > dw) ? (dw - ck_x) : 10;
                    int ch = (ck_y + 10 > dh) ? (dh - ck_y) : 10;
                    gfx_fill_rect(dx + ck_x, dy + ck_y, cw, ch, c);
                }
            }
        }

        gfx_draw_rect_outline(dx - 2, dy - 2, dw + 4, dh + 4, 1, 0x2A2D35);
        if (iv_rot_buf) {
            gfx_draw_rgb_bitmap_scaled(dx, dy, dw, dh, iv_rot_buf, iv_disp_w, iv_disp_h);
        } else {
            gfx_draw_rgb_bitmap_scaled(dx, dy, dw, dh, cur_img.pixels, cur_img.width, cur_img.height);
        }
    } else if (iv_img_count == 0) {
        gfx_draw_string_transparent(view_x + (view_w - 140) / 2, view_y + view_h / 2 - 8, "No BMP images found", 0x6D7079);
    } else {
        gfx_draw_string_transparent(view_x + (view_w - 120) / 2, view_y + view_h / 2 - 8, "Select an image", 0x6D7079);
    }

    // ── Status bar ──────────────────────────────────────────
    int sb_y = y + h - IV_STATUSBAR_H;
    gfx_fill_rect(x, sb_y, w, IV_STATUSBAR_H, 0x15171D);
    gfx_draw_hline(x, sb_y, w, 0x262830);

    char info[160]; int si = 0;
    if (img_loaded) {
        const char* fn = img_name;
        while (*fn && si < 60) info[si++] = *fn++;
        info[si++] = ' ';
        if (cur_img.width >= 100) info[si++] = '0' + cur_img.width / 100;
        if (cur_img.width >= 10) info[si++] = '0' + (cur_img.width / 10) % 10;
        info[si++] = '0' + cur_img.width % 10;
        info[si++] = 'x';
        if (cur_img.height >= 100) info[si++] = '0' + cur_img.height / 100;
        if (cur_img.height >= 10) info[si++] = '0' + (cur_img.height / 10) % 10;
        info[si++] = '0' + cur_img.height % 10;
        info[si++] = ' ';
        info[si++] = '(';
        float zpct = (fit_view ? 1.0f : cur_zoom) * 100.0f;
        int whole = (int)zpct;
        int frac = (int)((zpct - whole) * 10.0f);
        if (whole >= 100) { info[si++] = '1'; info[si++] = '0'; info[si++] = '0'; }
        else if (whole >= 10) { info[si++] = '0' + whole / 10; info[si++] = '0' + whole % 10; }
        else { info[si++] = '0' + whole; }
        if (frac > 0 && whole < 100) info[si++] = '.', info[si++] = '0' + frac;
        info[si++] = '%'; info[si++] = ')';
        if (iv_rotation) {
            info[si++] = ' '; info[si++] = 'R';
            if (iv_rotation >= 100) info[si++] = '0' + iv_rotation / 100;
            if (iv_rotation >= 10) info[si++] = '0' + (iv_rotation / 10) % 10;
            info[si++] = '0' + iv_rotation % 10;
            info[si++] = 0x80; /* degree symbol approximation */
        }
    } else {
        const char* txt = (iv_img_count > 0) ? "Select an image to view" : "No BMP images found";
        while (*txt && si < 60) info[si++] = *txt++;
    }
    info[si] = 0;
    gfx_draw_string_transparent(view_x + 8, sb_y + 3, info, 0x6D7079);

    if (iv_img_count > 0) {
        char cnt[32]; int ci = 0;
        itoa(iv_sel_idx + 1, cnt + ci);
        while (cnt[ci]) ci++;
        cnt[ci++] = '/';
        itoa(iv_img_count, cnt + ci);
        gfx_draw_string_transparent(x + w - 60, sb_y + 3, cnt, 0x8B949E);
    }

    prev_mbtn = mbtn;
}

static void draw_tb_btn(int x, int y, int w, int h, const char* lbl, int active, int hover) {
    uint32_t bg = active ? 0x2D4A6F : (hover ? 0x1F2933 : 0x15171D);
    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect_outline(x, y, w, h, 1, hover ? 0x4A5568 : 0x262830);
    int tw = 0; const char* p = lbl; while (*p) { tw++; p++; }
    gfx_draw_string_transparent(x + (w - tw * 6) / 2, y + (h - 10) / 2, lbl, 0xC9D1D9);
}

// ── Keyboard ─────────────────────────────────────────────────
static void iv_on_key(int id, char key) {
    (void)id;
    if (!img_loaded && key != 27) return;

    if (key == 27) {
        wm_close_window(iv_win_id);
        return;
    }

    // Refresh list
    if (KEY_MATCH(key, 'r') && !img_loaded) {
        scan_files();
        return;
    }
    // Rotate
    if (KEY_MATCH(key, 'R') || (KEY_MATCH(key, 'r') && img_loaded)) {
        if (img_loaded) iv_rotation = (iv_rotation + 90) % 360;
        return;
    }

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
    if (KEY_MATCH(key, '0') || KEY_MATCH(key, 'f') || KEY_MATCH(key, 'F')) {
        fit_view = !fit_view;
        if (fit_view) { pan_x = 0; pan_y = 0; }
        return;
    }
    // 100%
    if (KEY_MATCH(key, '1')) {
        fit_view = 0; zoom_idx = 4; cur_zoom = 1.0f; pan_x = 0; pan_y = 0;
        return;
    }
    // Pan with arrow keys
    if (!fit_view && img_loaded) {
        int step = 30;
        if (KEY_MATCH(key, KEY_LEFT) || KEY_MATCH(key, 'a') || KEY_MATCH(key, 'A')) pan_x += step;
        if (KEY_MATCH(key, KEY_RIGHT) || KEY_MATCH(key, 'd') || KEY_MATCH(key, 'D')) pan_x -= step;
        if (KEY_MATCH(key, KEY_UP) || KEY_MATCH(key, 'w') || KEY_MATCH(key, 'W')) pan_y += step;
        if (KEY_MATCH(key, KEY_DOWN) || KEY_MATCH(key, 's') || KEY_MATCH(key, 'S')) pan_y -= step;
    }
    // Previous image
    if (KEY_MATCH(key, KEY_PAGE_UP) || KEY_MATCH(key, 'p') || KEY_MATCH(key, 'P')) {
        if (iv_sel_idx > 0) { iv_sel_idx--; load_file(iv_images[iv_sel_idx].name); }
        return;
    }
    // Next image
    if (KEY_MATCH(key, KEY_PAGE_DOWN) || KEY_MATCH(key, 'n') || KEY_MATCH(key, 'N')) {
        if (iv_sel_idx < iv_img_count - 1) { iv_sel_idx++; load_file(iv_images[iv_sel_idx].name); }
        return;
    }
}

// ── Entry points ─────────────────────────────────────────────
static void open_impl(const char* name) {
    prev_mbtn = 0; iv_sel_idx = 0; dragging = 0; iv_rotation = 0;
    cur_img.pixels = 0; img_loaded = 0;
    scan_files();
    reset_view();

    if (name && name[0]) {
        for (int i = 0; i < iv_img_count; i++) {
            int match = 1;
            for (int j = 0; name[j] && iv_images[i].name[j]; j++)
                if (name[j] != iv_images[i].name[j]) { match = 0; break; }
            if (match) { iv_sel_idx = i; load_file(iv_images[i].name); break; }
        }
    }

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 760, win_h = 520;
    iv_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h + WM_TITLEBAR_H,
                   "BMP Viewer", 0x4D5059, iv_render, iv_on_key, 0);
}

void imgview_app(void) {
    open_impl(0);
}

void imgview_open(const char* name) {
    open_impl(name);
}

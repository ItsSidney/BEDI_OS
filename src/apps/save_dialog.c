#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "filesystem/filesystem.h"
#include "kernel/time/timer.h"
#include <stdint.h>
#include <string.h>

#define SD_WIN_W      520
#define SD_WIN_H      440
#define SD_LIST_H     200
#define SD_ROW_H      24
#define SD_BTNH        30
#define SD_MAX_ITEMS   128
#define SD_MAX_PATH    256

static int sd_win_id;
static char sd_filename[128];
static void (*sd_callback)(const char*);
static int sd_selected_idx;
static int sd_scroll;
static int sd_prev_mbtn;
static int sd_cursor_pos;
static int sd_list_focus;
static int sd_confirm_overwrite;
static char sd_overwrite_name[128];

static char sd_items[SD_MAX_ITEMS][64];
static int sd_item_types[SD_MAX_ITEMS];
static int sd_item_sizes[SD_MAX_ITEMS];
static int sd_item_count;
static int sd_dir_changed;

static void sd_refresh_list(void) {
    sd_item_count = 0;
    int curr = fs_get_current_dir();
    if (curr < 0) return;
    int count = fs_get_dir_count(curr);
    if (count > SD_MAX_ITEMS) count = SD_MAX_ITEMS;

    int dirs[SD_MAX_ITEMS], dir_count = 0;
    int files[SD_MAX_ITEMS], file_count = 0;

    for (int i = 0; i < count; i++) {
        char name[64]; int sz, tp; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(curr, i, name, &sz, &tp, &fl, &mt) == 0) {
            if (tp == 1) {
                if (dir_count < SD_MAX_ITEMS) {
                    int j = 0;
                    while (name[j] && j < 63) { sd_items[dir_count][j] = name[j]; j++; }
                    sd_items[dir_count][j] = 0;
                    sd_item_types[dir_count] = 1;
                    sd_item_sizes[dir_count] = 0;
                    dir_count++;
                }
            } else if (!(fl & (FS_FLAG_SYSTEM | FS_FLAG_HIDDEN))) {
                int idx = dir_count + file_count;
                if (idx < SD_MAX_ITEMS) {
                    int j = 0;
                    while (name[j] && j < 63) { sd_items[idx][j] = name[j]; j++; }
                    sd_items[idx][j] = 0;
                    sd_item_types[idx] = 0;
                    sd_item_sizes[idx] = sz;
                    file_count++;
                }
            }
        }
    }
    sd_item_count = dir_count + file_count;
}

static void format_size_sd(int bytes, char* buf) {
    if (bytes < 1024) { itoa(bytes, buf); int l = strlen(buf); buf[l++] = 'B'; buf[l] = 0; }
    else if (bytes < 1048576) { itoa(bytes / 1024, buf); int l = strlen(buf); buf[l++] = 'K'; buf[l] = 0; }
    else { itoa(bytes / 1048576, buf); int l = strlen(buf); buf[l++] = 'M'; buf[l] = 0; }
}

static void sd_do_save(void) {
    if (!sd_filename[0]) return;
    int exists = fs_exists(sd_filename);
    if (exists) {
        if (!sd_confirm_overwrite) {
            int i = 0;
            while (sd_filename[i] && i < 127) { sd_overwrite_name[i] = sd_filename[i]; i++; }
            sd_overwrite_name[i] = 0;
            sd_confirm_overwrite = 1;
            return;
        }
        sd_confirm_overwrite = 0;
    }
    char full[SD_MAX_PATH];
    fs_pwd(full, SD_MAX_PATH);
    int fl = strlen(full);
    if (fl > 0 && full[fl - 1] != '/') { full[fl] = '/'; full[fl + 1] = 0; fl++; }
    int fnl = strlen(sd_filename);
    for (int i = 0; i < fnl && fl + i < SD_MAX_PATH - 1; i++) full[fl + i] = sd_filename[i];
    full[fl + fnl] = 0;
    if (sd_callback) sd_callback(full);
    wm_close_window(sd_win_id);
}

static void sd_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gfx_fill_rect(x, y, w, h, 0x0D0E12);

    if (sd_confirm_overwrite) {
        int bx = x + 60, by = y + (h - 90) / 2, bw = w - 120, bh = 90;
        gfx_fill_rect(bx, by, bw, bh, 0x1D1F26);
        gfx_draw_rect_outline(bx, by, bw, bh, 1, 0x58A6FF);
        gfx_draw_string_transparent(bx + 20, by + 16, "Overwrite existing file?", 0xFFFFFF);
        gfx_draw_string_transparent(bx + 20, by + 36, sd_overwrite_name, 0xF0883E);
        int mx = mouse_get_x(), my = mouse_get_y();
        int ok_h = (mx >= bx+40 && mx < bx+100 && my >= by+58 && my < by+82);
        int no_h = (mx >= bx+110 && mx < bx+170 && my >= by+58 && my < by+82);
        gfx_fill_rect(bx+40, by+58, 60, 24, ok_h ? 0x58A6FF : 0x2D4A6F);
        gfx_draw_rect_outline(bx+40, by+58, 60, 24, 1, 0xFFFFFF);
        gfx_draw_string_transparent(bx+50, by+62, "Yes", 0xFFFFFF);
        gfx_fill_rect(bx+110, by+58, 60, 24, no_h ? 0x2D4A6F : 0x1D1F26);
        gfx_draw_rect_outline(bx+110, by+58, 60, 24, 1, 0x30363D);
        gfx_draw_string_transparent(bx+120, by+62, "No", 0xC9D1D9);
        if ((mouse_get_buttons() & 1) && !(sd_prev_mbtn & 1)) {
            if (ok_h) sd_do_save();
            sd_confirm_overwrite = 0;
        }
        sd_prev_mbtn = mouse_get_buttons();
        return;
    }

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int mclick = (mbtn & 1) && !(sd_prev_mbtn & 1);
    sd_prev_mbtn = mbtn;

    char cwd[SD_MAX_PATH];
    fs_pwd(cwd, SD_MAX_PATH);

    if (sd_dir_changed) {
        sd_refresh_list();
        sd_scroll = 0;
        sd_selected_idx = -1;
        sd_dir_changed = 0;
    }

    int title_h = 36;
    gfx_fill_rect(x + 4, y + 4, w - 8, title_h, 0x15171D);
    gfx_draw_rect_outline(x + 4, y + 4, w - 8, title_h, 1, 0x262830);
    gfx_draw_string_transparent(x + 20, y + 12, "Save As", 0xFFFFFF);

    int info_y = y + title_h + 8;
    gfx_draw_string_transparent(x + 20, info_y, cwd, 0x6D7079);
    gfx_draw_hline(x + 12, info_y + 18, w - 24, 0x262830);

    int toolbar_y = info_y + 26;
    int mx2 = mouse_get_x(), my2 = mouse_get_y();
    int up_x = x + 20, up_y = toolbar_y, up_w = 48;
    int newf_x = up_x + up_w + 6, newf_w = 66;
    int up_hover = (mx2 >= up_x && mx2 < up_x + up_w && my2 >= up_y && my2 < up_y + SD_BTNH);
    int nf_hover = (mx2 >= newf_x && mx2 < newf_x + newf_w && my2 >= up_y && my2 < up_y + SD_BTNH);
    gfx_fill_rect(up_x, up_y, up_w, SD_BTNH, up_hover ? 0x2D4A6F : 0x1D1F26);
    gfx_draw_rect_outline(up_x, up_y, up_w, SD_BTNH, 1, up_hover ? 0x58A6FF : 0x30363D);
    gfx_draw_string_transparent(up_x + 12, up_y + 7, "Up", 0xC9D1D9);
    gfx_fill_rect(newf_x, up_y, newf_w, SD_BTNH, nf_hover ? 0x2D4A6F : 0x1D1F26);
    gfx_draw_rect_outline(newf_x, up_y, newf_w, SD_BTNH, 1, nf_hover ? 0x3FB950 : 0x30363D);
    gfx_draw_string_transparent(newf_x + 8, up_y + 7, "+Folder", 0x3FB950);
    if (mclick && up_hover) { fs_cd(".."); sd_dir_changed = 1; mclick = 0; }
    if (mclick && nf_hover) {
        fs_mkdir("New Folder");
        sd_dir_changed = 1; mclick = 0;
    }

    int list_x = x + 20;
    int list_y = up_y + SD_BTNH + 8;
    int list_w = w - 40;
    int max_visible = SD_LIST_H / SD_ROW_H;
    int total = sd_item_count;
    if (sd_scroll > total - max_visible && total > max_visible) sd_scroll = total - max_visible;
    if (sd_scroll < 0) sd_scroll = 0;

    gfx_fill_rect(list_x, list_y, list_w, SD_LIST_H, 0x0D0E12);
    gfx_draw_rect_outline(list_x, list_y, list_w, SD_LIST_H, 1, 0x262830);

    for (int i = 0; i < total; i++) {
        if (i < sd_scroll || i >= sd_scroll + max_visible) continue;
        int iy = list_y + (i - sd_scroll) * SD_ROW_H;
        int hover = (mx >= list_x && mx < list_x + list_w && my >= iy && my < iy + SD_ROW_H);
        int sel = (i == sd_selected_idx);
        uint32_t bg = 0x0D0E12;
        if (sel) bg = 0x2D4A6F;
        else if (hover && sd_list_focus) bg = 0x1D1F26;
        else if (i % 2 == 1) bg = 0x0A0E14;
        gfx_fill_rect(list_x + 2, iy, list_w - 4, SD_ROW_H - 1, bg);
        int is_dir = sd_item_types[i];
        if (is_dir) {
            gfx_fill_rect(list_x + 8, iy + 5, 10, 10, 0xF0883E);
            gfx_draw_rect_outline(list_x + 8, iy + 5, 10, 10, 1, 0xE3B341);
        } else {
            gfx_fill_rect(list_x + 8, iy + 3, 10, 12, 0x3FB950);
        }
        gfx_draw_string_transparent(list_x + 24, iy + 4, sd_items[i], sel ? 0xFFFFFF : 0xC9D1D9);
        if (mclick && hover) {
            sd_list_focus = 1;
            if (is_dir) {
                fs_cd(sd_items[i]);
                sd_dir_changed = 1;
            } else {
                sd_selected_idx = i;
                int l = 0;
                while (sd_items[i][l] && l < 127) { sd_filename[l] = sd_items[i][l]; l++; }
                sd_filename[l] = 0;
                sd_cursor_pos = l;
            }
            mclick = 0;
        }
    }
    if (mclick && mx >= list_x && mx < list_x + list_w && my >= list_y && my < list_y + SD_LIST_H)
        sd_list_focus = 1;

    if (total > max_visible) {
        int sx = list_x + list_w - 6;
        int sh = SD_LIST_H;
        int th = (max_visible * sh) / total;
        if (th < 16) th = 16;
        int ty = list_y + (sd_scroll * (sh - th)) / (total - max_visible);
        gfx_fill_rect(sx, list_y, 4, sh, 0x1A1D23);
        gfx_fill_rect(sx, ty, 4, th, 0x4A5568);
    }

    int input_y = list_y + SD_LIST_H + 12;
    gfx_draw_string_transparent(x + 20, input_y, "File name:", sd_list_focus ? 0x58A6FF : 0x6D7079);
    int input_x = x + 20;
    int input_y2 = input_y + 16;
    int input_w = w - 40;
    int input_h = 28;
    gfx_fill_rect(input_x, input_y2, input_w, input_h, 0x15171D);
    gfx_draw_rect_outline(input_x, input_y2, input_w, input_h, 1, sd_list_focus ? 0x30363D : 0x21262D);
    gfx_draw_string_transparent(input_x + 8, input_y2 + 6, sd_filename, 0xC9D1D9);
    if (!sd_list_focus && (timer_get_ms() / 500) % 2 == 0) {
        int cxx = input_x + 8 + sd_cursor_pos * 8;
        if (cxx < input_x + input_w - 4) gfx_fill_rect(cxx, input_y2 + 4, 2, 18, 0x58A6FF);
    }
    if (mclick && mx >= input_x && mx < input_x + input_w && my >= input_y2 && my < input_y2 + input_h) {
        sd_list_focus = 0;
        int click_x = mx - input_x - 8;
        if (click_x < 0) click_x = 0;
        sd_cursor_pos = click_x / 8;
        int len = strlen(sd_filename);
        if (sd_cursor_pos > len) sd_cursor_pos = len;
    }

    int btn_y = input_y2 + input_h + 14;
    int btn_w = 80, btn_h = 32;
    int cancel_x = x + w - btn_w - 24;
    int save_x = cancel_x - btn_w - 12;

    int save_hover = (mx >= save_x && mx < save_x + btn_w && my >= btn_y && my < btn_y + btn_h);
    int cancel_hover = (mx >= cancel_x && mx < cancel_x + btn_w && my >= btn_y && my < btn_y + btn_h);
    uint32_t sbg = save_hover ? 0x58A6FF : 0x2D4A6F;
    gfx_fill_rect(save_x, btn_y, btn_w, btn_h, sbg);
    gfx_draw_rect_outline(save_x, btn_y, btn_w, btn_h, 1, save_hover ? 0xFFFFFF : 0x4A90E2);
    gfx_draw_string_transparent(save_x + 20, btn_y + 8, "Save", 0xFFFFFF);
    uint32_t cbg = cancel_hover ? 0x2D4A6F : 0x1D1F26;
    gfx_fill_rect(cancel_x, btn_y, btn_w, btn_h, cbg);
    gfx_draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, 1, cancel_hover ? 0x58A6FF : 0x30363D);
    gfx_draw_string_transparent(cancel_x + 14, btn_y + 8, "Cancel", 0xC9D1D9);

    if (mclick && save_hover && sd_filename[0]) { sd_do_save(); mclick = 0; }
    if (mclick && cancel_hover) { wm_close_window(sd_win_id); mclick = 0; }
}

static void sd_key_list(unsigned char k) {
    int total = sd_item_count;
    int max_visible = SD_LIST_H / SD_ROW_H;
    if (k == 128 || k == 'k') {
        if (sd_selected_idx > 0) sd_selected_idx--;
        if (sd_selected_idx < sd_scroll) sd_scroll = sd_selected_idx;
    } else if (k == 129 || k == 'j') {
        if (sd_selected_idx < total - 1) sd_selected_idx++;
        if (sd_selected_idx >= sd_scroll + max_visible) sd_scroll = sd_selected_idx - max_visible + 1;
    } else if (k == '\n') {
        if (sd_selected_idx >= 0 && sd_item_types[sd_selected_idx]) {
            fs_cd(sd_items[sd_selected_idx]);
            sd_dir_changed = 1;
        } else if (sd_selected_idx >= 0) {
            int l = 0;
            while (sd_items[sd_selected_idx][l] && l < 127) { sd_filename[l] = sd_items[sd_selected_idx][l]; l++; }
            sd_filename[l] = 0;
            sd_cursor_pos = l;
            sd_list_focus = 0;
        }
    }
}

static void sd_on_key(int id, char key) {
    (void)id;
    unsigned char k = (unsigned char)key;
    if (k == 27) {
        if (sd_confirm_overwrite) { sd_confirm_overwrite = 0; return; }
        wm_close_window(sd_win_id); return;
    }
    if (sd_confirm_overwrite) {
        if (k == '\n') sd_do_save();
        else sd_confirm_overwrite = 0;
        return;
    }
    if (k == '\t') { sd_list_focus = !sd_list_focus; return; }
    if (sd_list_focus) { sd_key_list(k); return; }
    if (k == '\n') { sd_do_save(); return; }
    if (k == 128 || k == 129) { sd_key_list(k); return; }
    if (k == KEY_LEFT) { if (sd_cursor_pos > 0) sd_cursor_pos--; return; }
    if (k == KEY_RIGHT) { int len = strlen(sd_filename); if (sd_cursor_pos < len) sd_cursor_pos++; return; }
    if (k == '\b') {
        int len = strlen(sd_filename);
        if (sd_cursor_pos > 0 && len > 0) {
            for (int i = sd_cursor_pos - 1; i < len - 1; i++) sd_filename[i] = sd_filename[i + 1];
            sd_filename[len - 1] = 0;
            sd_cursor_pos--;
        }
        return;
    }
    if (k >= 32 && k <= 126) {
        int len = strlen(sd_filename);
        if (len < 120 && sd_cursor_pos <= len) {
            for (int i = len; i > sd_cursor_pos; i--) sd_filename[i] = sd_filename[i - 1];
            sd_filename[sd_cursor_pos] = k;
            sd_filename[len + 1] = 0;
            sd_cursor_pos++;
        }
    }
}

void save_dialog_open(const char* default_name, void (*callback)(const char*)) {
    sd_callback = callback;
    sd_selected_idx = -1;
    sd_scroll = 0;
    sd_prev_mbtn = 0;
    sd_dir_changed = 1;
    sd_cursor_pos = 0;
    sd_list_focus = 0;
    sd_confirm_overwrite = 0;

    int l = 0;
    while (default_name[l] && l < 127) { sd_filename[l] = default_name[l]; l++; }
    sd_filename[l] = 0;
    sd_cursor_pos = l;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_x = (fw - SD_WIN_W) / 2;
    int win_y = (fh - SD_WIN_H) / 2;
    sd_win_id = wm_open_window(win_x, win_y, SD_WIN_W, SD_WIN_H + WM_TITLEBAR_H,
                   "Save File", 0x4D5059, sd_render, sd_on_key, 0);
}

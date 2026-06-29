#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "filesystem/filesystem.h"
#include <stdint.h>
#include <string.h>

#define LD_WIN_W     520
#define LD_WIN_H     400
#define LD_LIST_H    240
#define LD_ROW_H     24
#define LD_BTNH       30
#define LD_MAX_ITEMS  128
#define LD_MAX_PATH   256

static int ld_win_id;
static void (*ld_callback)(const char*);
static int ld_selected, ld_scroll, ld_prev_mbtn;
static int ld_item_count;

static char ld_items[LD_MAX_ITEMS][64];
static int ld_item_types[LD_MAX_ITEMS];
static int ld_dir_changed;

static void ld_refresh(void) {
    ld_item_count = 0;
    int curr = fs_get_current_dir();
    if (curr < 0) return;
    int count = fs_get_dir_count(curr);
    if (count > LD_MAX_ITEMS) count = LD_MAX_ITEMS;
    for (int i = 0; i < count; i++) {
        char name[64]; int sz, tp; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(curr, i, name, &sz, &tp, &fl, &mt) == 0) {
            if (tp == 1 || !(fl & (2 | 4))) {
                int j = 0;
                while (name[j] && j < 63) { ld_items[ld_item_count][j] = name[j]; j++; }
                ld_items[ld_item_count][j] = 0;
                ld_item_types[ld_item_count] = tp;
                ld_item_count++;
            }
        }
    }
}

static void ld_do_open(void) {
    if (ld_selected < 0 || ld_selected >= ld_item_count) return;
    if (ld_item_types[ld_selected] == 1) {
        fs_cd(ld_items[ld_selected]);
        ld_dir_changed = 1;
        ld_selected = -1;
        ld_scroll = 0;
        return;
    }
    char full[LD_MAX_PATH];
    fs_pwd(full, LD_MAX_PATH);
    int fl = strlen(full);
    if (fl > 0 && full[fl - 1] != '/') { full[fl] = '/'; full[fl + 1] = 0; fl++; }
    for (int i = 0; ld_items[ld_selected][i] && fl < LD_MAX_PATH - 1; i++) full[fl++] = ld_items[ld_selected][i];
    full[fl] = 0;
    if (ld_callback) ld_callback(full);
    wm_close_window(ld_win_id);
}

static void ld_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    gfx_fill_rect(x, y, w, h, 0x0D0E12);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int mclick = (mbtn & 1) && !(ld_prev_mbtn & 1);
    ld_prev_mbtn = mbtn;

    char cwd[LD_MAX_PATH];
    fs_pwd(cwd, LD_MAX_PATH);

    if (ld_dir_changed) { ld_refresh(); ld_scroll = 0; ld_selected = -1; ld_dir_changed = 0; }

    // Title
    int title_h = 36;
    gfx_fill_rect(x + 4, y + 4, w - 8, title_h, 0x15171D);
    gfx_draw_rect_outline(x + 4, y + 4, w - 8, title_h, 1, 0x262830);
    gfx_draw_string_transparent(x + 20, y + 12, "Open File", 0xFFFFFF);

    // Path display
    int info_y = y + title_h + 8;
    gfx_draw_string_transparent(x + 20, info_y, cwd, 0x6D7079);
    gfx_draw_hline(x + 12, info_y + 18, w - 24, 0x262830);

    // Up button
    int toolbar_y = info_y + 26;
    int up_x = x + 20, up_y = toolbar_y, up_w = 48;
    int up_hover = (mx >= up_x && mx < up_x + up_w && my >= up_y && my < up_y + LD_BTNH);
    gfx_fill_rect(up_x, up_y, up_w, LD_BTNH, up_hover ? 0x2D4A6F : 0x1D1F26);
    gfx_draw_rect_outline(up_x, up_y, up_w, LD_BTNH, 1, up_hover ? 0x58A6FF : 0x30363D);
    gfx_draw_string_transparent(up_x + 12, up_y + 7, "Up", 0xC9D1D9);
    if (mclick && up_hover) { fs_cd(".."); ld_dir_changed = 1; mclick = 0; }

    // File list
    int list_x = x + 20;
    int list_y = up_y + LD_BTNH + 8;
    int list_w = w - 40;
    int max_vis = LD_LIST_H / LD_ROW_H;
    int total = ld_item_count;
    if (ld_scroll > total - max_vis && total > max_vis) ld_scroll = total - max_vis;
    if (ld_scroll < 0) ld_scroll = 0;

    gfx_fill_rect(list_x, list_y, list_w, LD_LIST_H, 0x0D0E12);
    gfx_draw_rect_outline(list_x, list_y, list_w, LD_LIST_H, 1, 0x262830);

    for (int i = 0; i < total; i++) {
        if (i < ld_scroll || i >= ld_scroll + max_vis) continue;
        int iy = list_y + (i - ld_scroll) * LD_ROW_H;
        int hover = (mx >= list_x && mx < list_x + list_w && my >= iy && my < iy + LD_ROW_H);
        uint32_t bg = 0x0D0E12;
        if (i == ld_selected) bg = 0x2D4A6F;
        else if (hover) bg = 0x1D1F26;
        else if (i % 2 == 1) bg = 0x0A0E14;
        gfx_fill_rect(list_x + 2, iy, list_w - 4, LD_ROW_H - 1, bg);
        if (ld_item_types[i] == 1) {
            gfx_fill_rect(list_x + 8, iy + 5, 10, 10, 0xF0883E);
            gfx_draw_rect_outline(list_x + 8, iy + 5, 10, 10, 1, 0xE3B341);
        } else {
            gfx_fill_rect(list_x + 8, iy + 3, 10, 12, 0x3FB950);
        }
        gfx_draw_string_transparent(list_x + 24, iy + 4, ld_items[i], (i == ld_selected) ? 0xFFFFFF : 0xC9D1D9);
        if (mclick && hover) {
            if (ld_item_types[i] == 1) {
                fs_cd(ld_items[i]);
                ld_dir_changed = 1;
            } else {
                ld_selected = i;
            }
            mclick = 0;
        }
        if (mclick && hover && ld_item_types[i] == 1) { mclick = 0; }
    }

    // Scrollbar
    if (total > max_vis) {
        int sx = list_x + list_w - 6, sh = LD_LIST_H;
        int th = max_vis * sh / total;
        if (th < 16) th = 16;
        int ty = list_y + ld_scroll * (sh - th) / (total - max_vis);
        gfx_fill_rect(sx, list_y, 4, sh, 0x1A1D23);
        gfx_fill_rect(sx, ty, 4, th, 0x4A5568);
    }

    // Buttons
    int btn_y = list_y + LD_LIST_H + 14;
    int btn_w = 80, btn_h = 32;
    int cancel_x = x + w - btn_w - 24;
    int open_x = cancel_x - btn_w - 12;

    int open_hover = (mx >= open_x && mx < open_x + btn_w && my >= btn_y && my < btn_y + btn_h);
    int cancel_hover = (mx >= cancel_x && mx < cancel_x + btn_w && my >= btn_y && my < btn_y + btn_h);
    uint32_t obg = open_hover ? 0x58A6FF : 0x2D4A6F;
    gfx_fill_rect(open_x, btn_y, btn_w, btn_h, obg);
    gfx_draw_rect_outline(open_x, btn_y, btn_w, btn_h, 1, open_hover ? 0xFFFFFF : 0x4A90E2);
    gfx_draw_string_transparent(open_x + 18, btn_y + 8, "Open", 0xFFFFFF);
    uint32_t cbg = cancel_hover ? 0x2D4A6F : 0x1D1F26;
    gfx_fill_rect(cancel_x, btn_y, btn_w, btn_h, cbg);
    gfx_draw_rect_outline(cancel_x, btn_y, btn_w, btn_h, 1, cancel_hover ? 0x58A6FF : 0x30363D);
    gfx_draw_string_transparent(cancel_x + 14, btn_y + 8, "Cancel", 0xC9D1D9);

    if (mclick && open_hover && ld_selected >= 0) { ld_do_open(); mclick = 0; }
    if (mclick && cancel_hover) { wm_close_window(ld_win_id); mclick = 0; }
}

static void ld_on_key(int id, char key) {
    (void)id;
    unsigned char k = (unsigned char)key;
    if (k == 27) { wm_close_window(ld_win_id); return; }
    if (k == '\n') { ld_do_open(); return; }
    if (k == 128 || k == 'k') {
        if (ld_selected > 0) ld_selected--;
        if (ld_selected < ld_scroll) ld_scroll = ld_selected;
        return;
    }
    if (k == 129 || k == 'j') {
        int max_vis = LD_LIST_H / LD_ROW_H;
        if (ld_selected < ld_item_count - 1) ld_selected++;
        if (ld_selected >= ld_scroll + max_vis) ld_scroll = ld_selected - max_vis + 1;
        return;
    }
}

void load_dialog_open(void (*callback)(const char*)) {
    ld_callback = callback;
    ld_selected = -1;
    ld_scroll = 0;
    ld_prev_mbtn = 0;
    ld_dir_changed = 1;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_x = (fw - LD_WIN_W) / 2;
    int win_y = (fh - LD_WIN_H) / 2;
    ld_win_id = wm_open_window(win_x, win_y, LD_WIN_W, LD_WIN_H + WM_TITLEBAR_H,
                   "Open File", 0x3FB950, ld_render, ld_on_key, 0);
}

#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "gui/gui.h"
#include "gui/wm.h"
#include "filesystem/filesystem.h"
#include "apps/text_editor.h"
#include "apps/imgview.h"
#include "apps/hexdump.h"
#include <string.h>
#include <stdint.h>

#define MAX_FILES 256
#define ROW_H     26
#define TOOLBAR_H 32
#define PATHBAR_H 24
#define STATUSBAR_H 24
#define CONTENT_TOP (TOOLBAR_H + PATHBAR_H)
#define COL_HDR_H 22

typedef struct { char name[64]; int size; int type; uint8_t flags; uint32_t mod_time; } fe_file_t;
static fe_file_t fe_files[MAX_FILES];
static int fe_count, fe_sel, fe_scroll, fe_win_id = -1;
static int fe_prev_mouse, fe_sort_asc = 1;
static char fe_path[256];
static int fe_hover = -1;

static void fe_refresh(void);
static void fe_navigate_to(const char* name);

#define HISTORY_MAX 32
static char fe_history[HISTORY_MAX][256];
static int fe_history_pos = 0;
static int fe_history_count = 0;

static void get_node_path(int node_id, char* out, int out_size) {
    char segs[10][64]; int sc = 0, d = node_id;
    while (d >= 0 && sc < 10) {
        char name[64]; int sz, tp, par; uint8_t fl; uint32_t mt;
        if (fs_get_node(d, name, &sz, &tp, &par, &fl, &mt) != 0) break;
        strcpy(segs[sc++], name); d = par;
    }
    int pi = 0;
    if (sc == 0) { out[0] = '/'; out[1] = 0; return; }
    for (int s = sc - 1; s >= 0; s--) {
        out[pi++] = '/';
        for (int j = 0; segs[s][j] && pi < out_size - 1; j++) out[pi++] = segs[s][j];
    }
    out[pi] = 0;
}

static void fe_go_back(void) {
    if (fe_history_pos > 0) {
        fe_history_pos--;
        fs_cd(fe_history[fe_history_pos]);
        fe_refresh();
    }
}

static void fe_go_forward(void) {
    if (fe_history_pos < fe_history_count - 1) {
        fe_history_pos++;
        fs_cd(fe_history[fe_history_pos]);
        fe_refresh();
    }
}

static void fe_navigate_to(const char* name) {
    fe_history_count = fe_history_pos + 1;
    if (name[0] == '/') fs_cd(name);
    else if (strcmp(name, "..") == 0) fs_cd("..");
    else fs_cd(name);
    fe_refresh();
    if (fe_history_count == 0 || strcmp(fe_history[fe_history_count - 1], fe_path) != 0) {
        strcpy(fe_history[fe_history_count], fe_path);
        fe_history_count++;
        fe_history_pos = fe_history_count - 1;
    }
}

static int ends_with(const char* n, const char* e) {
    int nl = strlen(n), el = strlen(e);
    return (nl >= el && strcmp(n + nl - el, e) == 0);
}
static int is_txt(const char* n) { return ends_with(n, ".txt"); }
static int is_bc(const char* n) { return ends_with(n, ".bc"); }
static int is_bin(const char* n) { return ends_with(n, ".bin"); }
static int is_bmp(const char* n) { return ends_with(n, ".bmp") || ends_with(n, ".BMP"); }
static int is_cfile(const char* n) { return ends_with(n, ".c") || ends_with(n, ".h"); }
static int is_asm(const char* n) { return ends_with(n, ".asm"); }

static void format_size(int bytes, char* buf) {
    if (bytes < 1024) { itoa(bytes, buf); int l = strlen(buf); buf[l]=' '; buf[l+1]='B'; buf[l+2]=0; }
    else if (bytes < 1048576) { itoa(bytes / 1024, buf); int l = strlen(buf); buf[l]=' '; buf[l+1]='K'; buf[l+2]='B'; buf[l+3]=0; }
    else { itoa(bytes / 1048576, buf); int l = strlen(buf); buf[l]=' '; buf[l+1]='M'; buf[l+2]='B'; buf[l+3]=0; }
}

static void fe_sort(void) {
    for (int i = 0; i < fe_count - 1; i++)
        for (int j = 0; j < fe_count - 1 - i; j++) {
            int r;
            if (fe_files[j].type != fe_files[j+1].type)
                r = fe_files[j].type - fe_files[j+1].type;
            else
                r = strcmp(fe_files[j].name, fe_files[j+1].name);
            if ((fe_sort_asc ? r : -r) > 0) {
                fe_file_t t = fe_files[j]; fe_files[j] = fe_files[j+1]; fe_files[j+1] = t;
            }
        }
}

static void fe_load(void) {
    fe_count = 0;
    int cur = fs_get_current_dir();
    if (cur < 0) return;
    int cnt = fs_get_dir_count(cur);
    if (cnt > MAX_FILES) cnt = MAX_FILES;
    for (int i = 0; i < cnt; i++) {
        char name[64]; int sz, tp; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(cur, i, name, &sz, &tp, &fl, &mt) == 0) {
            if (tp != 1 && (fl & (2 | 4))) continue;
            fe_file_t* e = &fe_files[fe_count++];
            strcpy(e->name, name); e->size = sz; e->type = tp; e->flags = fl; e->mod_time = mt;
        }
    }
    fe_sort();
}

static void fe_refresh(void) {
    fe_load(); fe_sel = -1; fe_scroll = 0;
    fs_pwd(fe_path, 256);
}

static void fe_cd(const char* name) {
    if (name[0] == '/') fs_cd(name);
    else if (strcmp(name, "..") == 0) fs_cd("..");
    else fs_cd(name);
    fe_refresh();
}

static void fe_open(void) {
    if (fe_sel < 0 || fe_sel >= fe_count) return;
    const char* n = fe_files[fe_sel].name;
    if (fe_files[fe_sel].type == 1) { fe_navigate_to(n); return; }
    if (is_bmp(n)) { imgview_open(n); return; }
    if (is_bin(n)) { extern void brun_main(char*); brun_main((char*)n); return; }
    if (is_txt(n) || is_cfile(n) || is_asm(n) || is_bc(n)) { text_editor_open(n); return; }
    hexdump_open(n);
}

static void fe_draw_icon(int x, int y, const char* name, int is_dir) {
    if (is_dir) {
        gfx_fill_rect(x+2, y+8, 16, 11, 0xEBCB8B);
        gfx_draw_rect_outline(x+2, y+8, 16, 11, 1, 0xD4A74D);
        gfx_fill_rect(x+4, y+8, 4, 3, 0xD4A74D);
        return;
    }
    uint32_t c = 0x58A6FF;
    if (is_txt(name) || is_cfile(name)) c = 0xE6EDF3;
    else if (is_bin(name)) c = 0x3FB950;
    else if (is_bmp(name)) c = 0x22D3EE;
    else if (is_bc(name)) c = 0xBC8CFF;
    else if (is_asm(name)) c = 0x6E7681;
    gfx_fill_rect(x+2, y+2, 18, 20, 0x161B22);
    gfx_draw_rect_outline(x+2, y+2, 18, 20, 1, c);
    gfx_fill_circle(x+11, y+7, 4, c);
    gfx_fill_rect(x+7, y+13, 8, 6, c);
}

static void fe_draw_toolbar_icon(int bx, int by, int idx, uint32_t color) {
    int cx = bx + 14, cy = by + 14;
    if (idx == 0) { // Back - left arrow
        gfx_draw_line(cx-5, cy, cx+4, cy-5, color);
        gfx_draw_line(cx-5, cy, cx+4, cy+5, color);
        gfx_draw_line(cx-5, cy, cx+4, cy, color);
    } else if (idx == 1) { // Forward - right arrow
        gfx_draw_line(cx+5, cy, cx-4, cy-5, color);
        gfx_draw_line(cx+5, cy, cx-4, cy+5, color);
        gfx_draw_line(cx+5, cy, cx-4, cy, color);
    } else if (idx == 2) { // Up - up arrow
        gfx_draw_line(cx, cy-6, cx, cy+5, color);
        gfx_draw_line(cx, cy-6, cx-4, cy-2, color);
        gfx_draw_line(cx, cy-6, cx+4, cy-2, color);
    } else if (idx == 3) { // Refresh - circular arrow
        gfx_draw_circle(cx, cy-1, 5, color);
        gfx_draw_line(cx+2, cy-7, cx+5, cy-5, color);
        gfx_draw_line(cx+5, cy-5, cx+2, cy-4, color);
    } else if (idx == 4) { // Home - house
        gfx_draw_line(cx, cy-7, cx-6, cy-1, color);
        gfx_draw_line(cx, cy-7, cx+6, cy-1, color);
        gfx_draw_line(cx-6, cy-1, cx+6, cy-1, color);
        gfx_draw_rect_outline(cx-4, cy-1, 8, 7, 1, color);
    } else if (idx == 5) { // Root - folder
        gfx_draw_rect_outline(cx-6, cy-3, 12, 8, 1, color);
        gfx_draw_line(cx-6, cy-3, cx-3, cy-6, color);
        gfx_draw_line(cx-3, cy-6, cx+2, cy-6, color);
        gfx_draw_line(cx+2, cy-6, cx+6, cy-3, color);
    }
}

static void fe_draw_toolbar(int x, int y, int w) {
    gfx_fill_rect(x, y, w, TOOLBAR_H, 0x0D1117);
    gfx_draw_hline(x, y + TOOLBAR_H - 1, w, 0x21262D);
    int mx = mouse_get_x(), my = mouse_get_y();
    int disabled[6] = {0};
    if (fe_history_pos <= 0) disabled[0] = 1;
    if (fe_history_pos >= fe_history_count - 1) disabled[1] = 1;
    for (int i = 0; i < 6; i++) {
        int bx = x + 6 + i * 32;
        int hover = (mx >= bx && mx < bx + 28 && my >= y+2 && my < y+30) && !disabled[i];
        uint32_t bg = 0x0D1117;
        if (disabled[i]) bg = 0x090B10;
        else if (hover) bg = 0x1A437A;
        gfx_fill_rect(bx, y+2, 28, 28, bg);
        gfx_draw_rect_outline(bx, y+2, 28, 28, 1, disabled[i] ? 0x161B22 : (hover ? 0x1F6FEB : 0x21262D));
        uint32_t ic = disabled[i] ? 0x30363D : (hover ? 0xFFFFFF : 0xC9D1D9);
        fe_draw_toolbar_icon(bx, y+2, i, ic);
    }
}

static void fe_draw_pathbar(int x, int y, int w) {
    gfx_fill_rect(x, y, w, PATHBAR_H, 0x0D1117);
    gfx_draw_hline(x, y + PATHBAR_H - 1, w, 0x21262D);
    gfx_draw_string_transparent(x+8, y+4, fe_path, 0xCCCCCC);
}

static void fe_draw_list(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, 0x0D1117);
    int cols[4] = {4, w*34/100, w*48/100, w*62/100};
    int col_w[4];
    col_w[0] = cols[1] - cols[0];
    col_w[1] = cols[2] - cols[1];
    col_w[2] = cols[3] - cols[2];
    col_w[3] = w - cols[3] - 4;
    // Column headers
    gfx_fill_rect(x, y, w, COL_HDR_H, 0x161B22);
    gfx_draw_hline(x, y + COL_HDR_H - 1, w, 0x30363D);
    const char* hdrs[] = {"Name", "Size", "Type", "Modified"};
    for (int c = 0; c < 4; c++) {
        gfx_draw_string_transparent(x + cols[c] + 4, y + 3, hdrs[c], 0x8B949E);
        char ind = fe_sort_asc ? 24 : 25;
        if (c == 0) { char s[2] = {ind, 0}; gfx_draw_string_transparent(x + cols[c] + 34, y + 3, s, 0x1F6FEB); }
        if (c < 3) gfx_draw_vline(x + cols[c+1] - 1, y, COL_HDR_H, 0x21262D);
    }
    int list_y = y + COL_HDR_H;
    int body_h = h - COL_HDR_H;
    int vis = body_h / ROW_H;
    if (vis < 1) vis = 1;
    int mx = mouse_get_x(), my = mouse_get_y();
    fe_hover = -1;
    for (int r = 0; r < vis; r++) {
        int idx = fe_scroll + r;
        if (idx >= fe_count) break;
        int ry = list_y + r * ROW_H;
        int hover = (mx >= x && mx < x + w && my >= ry && my < ry + ROW_H);
        if (hover) fe_hover = idx;
        uint32_t bg = 0x0D1117;
        if (idx == fe_sel) bg = 0x1A437A;
        else if (hover) bg = 0x161B22;
        else if (r % 2) bg = 0x0A0E14;
        gfx_fill_rect(x+2, ry, w-4, ROW_H, bg);
        fe_file_t* e = &fe_files[idx];
        int is_dir = (e->type == 1);
        fe_draw_icon(x+6, ry+4, e->name, is_dir);
        uint32_t nc = (idx == fe_sel) ? 0xFFFFFF : (is_dir ? 0x58A6FF : 0xC9D1D9);
        char nd[40];
        int nl = strlen(e->name), mc = (col_w[0] - 28) / 8;
        if (mc < 4) mc = 4;
        if (nl > mc) { memcpy(nd, e->name, mc-3); nd[mc-3]='.'; nd[mc-2]='.'; nd[mc-1]='.'; nd[mc]=0; }
        else strcpy(nd, e->name);
        gfx_draw_string_transparent(x + cols[0] + 26, ry + 5, nd, nc);
        char buf[24];
        if (col_w[1] > 40 && !is_dir) { format_size(e->size, buf); gfx_draw_string_transparent(x + cols[1] + 4, ry + 5, buf, 0x8B949E); }
        if (col_w[2] > 50) {
            const char* tn = "Binary";
            if (is_dir) tn = "Folder";
            else if (is_txt(e->name)) tn = "Text";
            else if (is_bc(e->name)) tn = "Bedi-C";
            else if (is_bin(e->name)) tn = "Executable";
            else if (is_bmp(e->name)) tn = "Bitmap";
            else if (is_cfile(e->name)) tn = ends_with(e->name, ".c") ? "C Source" : "C Header";
            else if (is_asm(e->name)) tn = "Assembly";
            gfx_draw_string_transparent(x + cols[2] + 4, ry + 5, tn, 0x6E7681);
        }
        if (col_w[3] > 60) { fs_format_time(e->mod_time, buf, 24); gfx_draw_string_transparent(x + cols[3] + 4, ry + 5, buf, 0x6E7681); }
    }
}

static void fe_draw_status(int x, int y, int w) {
    gfx_fill_rect(x, y, w, STATUSBAR_H, 0x161B22);
    gfx_draw_hline(x, y, w, 0x30363D);
    char s[32]; int si = 0;
    itoa(fe_count, s + si); while (s[si]) si++;
    s[si++] = ' '; s[si++] = 'i'; s[si++] = 't'; s[si++] = 'e'; s[si++] = 'm'; s[si] = 0;
    gfx_draw_string_transparent(x+8, y+4, s, 0x8B949E);
}

static void fe_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy; (void)id;
    gfx_fill_rect(x, y, w, h, 0x0D1117);
    int body_h = h - CONTENT_TOP - STATUSBAR_H;
    if (body_h < 20) body_h = 20;
    fe_draw_toolbar(x, y, w);
    fe_draw_pathbar(x, y + TOOLBAR_H, w);
    fe_draw_list(x, y + CONTENT_TOP, w, body_h);
    fe_draw_status(x, y + h - STATUSBAR_H, w);

    int mbtn = mouse_get_buttons();
    int lc = (mbtn & 1) && !(fe_prev_mouse & 1);
    int rc = (mbtn & 2) && !(fe_prev_mouse & 2);
    fe_prev_mouse = mbtn;
    if (!lc && !rc) return;

    int mx = mouse_get_x(), my = mouse_get_y();

    // Toolbar clicks
    if (my >= y && my < y + TOOLBAR_H) {
        for (int i = 0; i < 6; i++) {
            int bx = x + 6 + i * 32;
            if (mx >= bx && mx < bx + 28) {
                if (i == 0) fe_go_back();
                else if (i == 1) fe_go_forward();
                else if (i == 2) fe_navigate_to("..");
                else if (i == 3) fe_refresh();
                else if (i == 4) {
                    char home_path[256];
                    int home = fs_get_home_dir();
                    if (home >= 0) { get_node_path(home, home_path, 256); fe_navigate_to(home_path); }
                    else fe_navigate_to("/");
                }
                else if (i == 5) fe_navigate_to("/");
                return;
            }
        }
        return;
    }

    int list_top = y + CONTENT_TOP + COL_HDR_H;
    int list_h = body_h - COL_HDR_H;
    if (list_h < 0) list_h = 0;
    if (mx >= x && mx < x + w && my >= list_top && my < list_top + list_h) {
        int row = (my - list_top) / ROW_H;
        int idx = fe_scroll + row;
        if (idx >= 0 && idx < fe_count) {
            if (lc) {
                if (idx == fe_sel) fe_open();
                else fe_sel = idx;
            }
            if (rc) fe_open();
        }
    }
}

static void fe_on_key(int id, char key_in) {
    (void)id;
    unsigned char k = (unsigned char)key_in;
    if (k == 27) wm_close_window(fe_win_id);
    if (k == '\n') fe_open();
    if (k == '\b') fe_navigate_to("..");
    if (k == KEY_UP || k == 128) {
        if (fe_sel > 0) fe_sel--;
        if (fe_sel < fe_scroll) fe_scroll = fe_sel;
    }
    if (k == KEY_DOWN || k == 129) {
        if (fe_sel < fe_count - 1) fe_sel++;
        wm_window_t* win = wm_get_window(fe_win_id);
        int vis = 10;
        if (win) { int bb = win->h - WM_TITLEBAR_H - CONTENT_TOP - STATUSBAR_H - COL_HDR_H; vis = bb / ROW_H; }
        if (vis < 1) vis = 1;
        if (fe_sel >= fe_scroll + vis) fe_scroll = fe_sel - vis + 1;
    }
    if (k == KEY_PAGE_UP || k == 133) { fe_scroll -= 10; if (fe_scroll < 0) fe_scroll = 0; fe_sel = fe_scroll; }
    if (k == KEY_PAGE_DOWN || k == 134) {
        fe_scroll += 10; if (fe_scroll > fe_count - 1) fe_scroll = fe_count - 1;
        if (fe_scroll < 0) fe_scroll = 0; fe_sel = fe_scroll;
    }
}

static void fe_on_resize(int win_id, int w, int h) { (void)win_id; (void)w; (void)h; }

void file_explorer(void) {
    fe_count = 0; fe_sel = -1; fe_scroll = 0; fe_prev_mouse = 0;
    fe_hover = -1; fe_sort_asc = 1;
    fe_history_pos = 0; fe_history_count = 0;
    int home = fs_get_home_dir();
    if (home >= 0) {
        char path[256];
        get_node_path(home, path, 256);
        fs_cd(path);
    } else fs_cd("/");
    fe_refresh();
    strcpy(fe_history[0], fe_path);
    fe_history_count = 1;
    fe_history_pos = 0;
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 820, win_h = 540;
    if (win_w > (int)fw) win_w = fw;
    if (win_h > (int)fh) win_h = fh;
    fe_win_id = wm_open_window((fw-win_w)/2, (fh-win_h)/2, win_w, win_h,
        "File Explorer", 0x58A6FF, fe_on_render, fe_on_key, fe_on_resize);
}

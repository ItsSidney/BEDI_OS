// ============================================================
//  BEDI OS — Text Editor
//  Professional editor with line numbers, toolbar, cut/copy/paste,
//  find/replace, Home/End navigation, Tab insertion, Ctrl+S save,
//  auto-scroll, and status bar with line/col/size info.
// ============================================================
#include <stdint.h>
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "filesystem/filesystem.h"
#include "commands/commands.h"
#include "apps/save_dialog.h"

static char ed_filename[64];
static char ed_content[1048576];
static int ed_content_len = 0;
static int ed_cursor = 0;
static int ed_scroll_x = 0;
static int ed_scroll_y = 0;
static int ed_win_id = -1;
static int ed_drag_selecting = 0;
static int ed_press_x = -1, ed_press_y = -1;
static int ed_drag_hsb = 0;
static int ed_drag_vsb = 0;
static int ed_dirty = 0;
static int ed_selection_start = -1;
static int ed_naming = 0;
static char ed_name_buf[64];
static int ed_name_len = 0;

/* Clipboard */
static char ed_clipboard[4096];
static int ed_clip_len = 0;
static int ed_clip_action = 0; /* 0=none, 1=copy, 2=cut, 3=paste */
static uint32_t ed_clip_action_time = 0;
#define ED_CLIP_INDICATOR_MS 800

/* Find */
static int ed_find_active = 0;
static char ed_find_buf[64];
static int ed_find_len = 0;
static int ed_find_pos = -1;

#define ED_LINE_H 16
#define ED_CHAR_W 8
#define ED_MAX_SIZE 1048575
#define ED_GUTTER_W 44
#define ED_TOOLBAR_H 28
#define ED_VSB_W 10

#define BTN_NEW   1
#define BTN_OPEN  2
#define BTN_SAVE  3
#define BTN_CUT   4
#define BTN_COPY  5
#define BTN_PASTE 6
#define BTN_FIND  7

extern uint32_t timer_get_ms(void);
static void ed_on_render(int id, int x, int y, int w, int h, int vx, int vy);
static void ed_on_key(int id, char key_in);
static void ed_on_resize(int win_id, int w, int h);
static void ed_btn_new(int win_id, int btn_id);
static void ed_btn_save(int win_id, int btn_id);
static void ed_btn_cut(int win_id, int btn_id);
static void ed_btn_copy(int win_id, int btn_id);
static void ed_btn_paste(int win_id, int btn_id);
static void ed_btn_find(int win_id, int btn_id);
static void ed_clip_copy(void);
static void ed_clip_cut(void);
static void ed_clip_paste(void);
static void ed_find_next(void);
static void ed_do_save(const char* name);
static void ed_save(void);
static void ed_insert_char(char c);
static void ed_delete_char(void);
static void ed_delete_forward(void);
static int ed_get_visible_lines(int h);
static int ed_count_lines(void);
static void ed_get_cursor_pos(int* row, int* col);
static int ed_line_start(int line);
static int ed_line_length(int line);
static void ed_format_number(int n, char* buf, int width);
void text_editor_new(const char* filename);
void text_editor_open(const char* filename);

static int ed_get_visible_lines(int h) {
    int status_h = 22;
    int text_h = h - ED_TOOLBAR_H - status_h;
    int lines = text_h / ED_LINE_H;
    return (lines > 0) ? lines : 1;
}

static int ed_count_lines(void) {
    int lines = 1;
    for (int i = 0; i < ed_content_len; i++) {
        if (ed_content[i] == '\n') lines++;
    }
    return lines;
}

static void ed_get_cursor_pos(int* row, int* col) {
    int r = 0, c = 0;
    for (int i = 0; i < ed_cursor; i++) {
        if (ed_content[i] == '\n') { r++; c = 0; }
        else c++;
    }
    *row = r;
    *col = c;
}

static int ed_line_start(int line) {
    int l = 0, pos = 0;
    while (pos < ed_content_len && l < line) {
        if (ed_content[pos] == '\n') l++;
        pos++;
    }
    return pos;
}

static int ed_line_length(int line) {
    int start = ed_line_start(line);
    int len = 0;
    while (start + len < ed_content_len && ed_content[start + len] != '\n') len++;
    return len;
}

static void ed_format_number(int n, char* buf, int width) {
    char temp[8];
    int len = 0;
    if (n == 0) { temp[len++] = '0'; }
    else {
        int tmp = n;
        while (tmp > 0) { temp[len++] = (tmp % 10) + '0'; tmp /= 10; }
    }
    for (int j = 0; j < len / 2; j++) {
        char t = temp[j]; temp[j] = temp[len-1-j]; temp[len-1-j] = t;
    }
    temp[len] = 0;
    int pad = width - len;
    int bi = 0;
    while (pad > 0) { buf[bi++] = ' '; pad--; }
    for (int j = 0; j < len; j++) buf[bi++] = temp[j];
    buf[bi] = 0;
}

static void ed_clip_copy(void) {
    if (ed_selection_start < 0 || ed_selection_start >= ed_content_len) return;
    int start = ed_selection_start;
    int end = ed_cursor;
    if (start > end) { int t = start; start = end; end = t; }
    int len = end - start;
    if (len > 4095) len = 4095;
    for (int i = 0; i < len; i++) ed_clipboard[i] = ed_content[start + i];
    ed_clip_len = len;
    ed_clip_action = 1;
    ed_clip_action_time = timer_get_ms();
}

static void ed_clip_cut(void) {
    ed_clip_copy();
    if (ed_selection_start >= 0 && ed_selection_start < ed_content_len && ed_cursor != ed_selection_start) {
        int start = ed_selection_start;
        int end = ed_cursor;
        if (start > end) { int t = start; start = end; end = t; }
        for (int i = end; i < ed_content_len; i++) ed_content[start + i - end] = ed_content[i];
        ed_content_len -= (end - start);
        ed_cursor = start;
        ed_dirty = 1;
    }
    ed_selection_start = -1;
    ed_clip_action = 2;
    ed_clip_action_time = timer_get_ms();
}

static void ed_clip_paste(void) {
    if (ed_clip_len <= 0) return;
    if (ed_content_len + ed_clip_len > ED_MAX_SIZE) return;
    for (int i = ed_content_len; i > ed_cursor; i--) ed_content[i + ed_clip_len - 1] = ed_content[i - 1];
    for (int i = 0; i < ed_clip_len; i++) ed_content[ed_cursor + i] = ed_clipboard[i];
    ed_content_len += ed_clip_len;
    ed_cursor += ed_clip_len;
    ed_dirty = 1;
    ed_selection_start = -1;
    ed_clip_action = 3;
    ed_clip_action_time = timer_get_ms();
}

static void ed_find_next(void) {
    if (ed_find_len == 0) return;
    int start = ed_cursor + 1;
    if (start >= ed_content_len) start = 0;
    for (int i = start; i < ed_content_len; i++) {
        int match = 1;
        for (int j = 0; j < ed_find_len && i + j < ed_content_len; j++) {
            if (ed_content[i + j] != ed_find_buf[j]) { match = 0; break; }
        }
        if (match) { ed_cursor = i; ed_find_pos = i; return; }
    }
    for (int i = 0; i < start && i + ed_find_len <= ed_content_len; i++) {
        int match = 1;
        for (int j = 0; j < ed_find_len && i + j < ed_content_len; j++) {
            if (ed_content[i + j] != ed_find_buf[j]) { match = 0; break; }
        }
        if (match) { ed_cursor = i; ed_find_pos = i; return; }
    }
    ed_find_pos = -1;
}

static const char* basename(const char* path) {
    const char* p = path;
    const char* last = path;
    while (*p) {
        if (*p == '/') last = p + 1;
        p++;
    }
    return last;
}

static void ed_do_save(const char* name) {
    const char* bn = basename(name);
    fs_delete(bn);
    int fd = fs_create(bn);
    if (fd >= 0) {
        fs_write(fd, ed_content, ed_content_len);
        fs_close(fd);
        ed_dirty = 0;
        int i = 0;
        while (bn[i] && i < 63) { ed_filename[i] = bn[i]; i++; }
        ed_filename[i] = 0;
    }
}

static void ed_save(void) {
    if (strcmp(ed_filename, "untitled.txt") == 0) {
        char old_path[128];
        fs_pwd(old_path, 128);
        fs_cd("/home/user/Documents");
        ed_do_save("untitled.txt");
        fs_cd(old_path);
        return;
    }
    ed_do_save(ed_filename);
}

static void ed_insert_char(char c) {
    if (ed_content_len < ED_MAX_SIZE) {
        for (int i = ed_content_len; i > ed_cursor; i--)
            ed_content[i] = ed_content[i - 1];
        ed_content[ed_cursor] = c;
        ed_content_len++;
        ed_cursor++;
        ed_dirty = 1;
    }
}

static void ed_delete_char(void) {
    if (ed_cursor > 0) {
        for (int i = ed_cursor - 1; i < ed_content_len - 1; i++)
            ed_content[i] = ed_content[i + 1];
        ed_content_len--;
        ed_cursor--;
        ed_dirty = 1;
    }
}

static void ed_delete_forward(void) {
    if (ed_cursor < ed_content_len) {
        for (int i = ed_cursor; i < ed_content_len - 1; i++)
            ed_content[i] = ed_content[i + 1];
        ed_content_len--;
        ed_dirty = 1;
    }
}

static void ed_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;

    /* Toolbar background */
    gfx_fill_rect(x, y, w, ED_TOOLBAR_H, 0x161B22);
    gfx_draw_hline(x, y + ED_TOOLBAR_H - 1, w, 0x30363D);

    /* Toolbar buttons rendered by WM, just draw separator lines */
    int btn_edges[] = { 64, 108, 152, 200, 248, 296, 352 };
    for (int i = 0; i < 7; i++) {
        gfx_draw_vline(x + btn_edges[i], y + 6, ED_TOOLBAR_H - 12, 0x30363D);
    }

    /* Find bar */
    int find_bar_h = ed_find_active ? 22 : 0;
    
    int content_y = y + ED_TOOLBAR_H;
    int status_h = 22;
    int hsb_h = 10; /* horizontal scrollbar height */
    int text_h = h - ED_TOOLBAR_H - status_h - hsb_h - find_bar_h;
    int vis_lines = ed_get_visible_lines(h - hsb_h - find_bar_h);

    /* Background for gutter */
    gfx_fill_rect(x, content_y, ED_GUTTER_W, text_h, 0x0A0E14);
    gfx_draw_vline(x + ED_GUTTER_W - 1, content_y, text_h, 0x21262D);

    /* Content background */
    gfx_fill_rect(x + ED_GUTTER_W, content_y, w - ED_GUTTER_W - ED_VSB_W, text_h, 0x0D1117);

    int cursor_row, cursor_col;
    ed_get_cursor_pos(&cursor_row, &cursor_col);

    /* Auto-scroll */
    if (cursor_row < ed_scroll_y) ed_scroll_y = cursor_row;
    if (cursor_row >= ed_scroll_y + vis_lines) ed_scroll_y = cursor_row - vis_lines + 1;

    int max_chars_per_line = (w - ED_GUTTER_W - 14) / ED_CHAR_W;
    if (max_chars_per_line < 1) max_chars_per_line = 1;
    if (cursor_col < ed_scroll_x) ed_scroll_x = cursor_col;
    if (cursor_col >= ed_scroll_x + max_chars_per_line) ed_scroll_x = cursor_col - max_chars_per_line + 1;
    if (ed_scroll_x < 0) ed_scroll_x = 0;

    int vis_line = 0;
    int char_idx = 0;
    int line = 0;
    int col = 0;

    for (int i = 0; i < ed_content_len && line < ed_scroll_y; i++) {
        if (ed_content[i] == '\n') line++;
        char_idx = i + 1;
    }

    vis_line = 0;
    line = ed_scroll_y;
    col = 0;

    int content_x = x + ED_GUTTER_W + 6 - ed_scroll_x;
    int content_right = x + w - 10;
    if (content_right < content_x + ED_CHAR_W) content_right = content_x + ED_CHAR_W;

    /* Selection highlight */
    int sel_start = -1, sel_end = -1;
    if (ed_selection_start >= 0) {
        sel_start = ed_selection_start;
        sel_end = ed_cursor;
        if (sel_start > sel_end) { int t = sel_start; sel_start = sel_end; sel_end = t; }
    }

    char lnum[8];
    ed_format_number(line + 1, lnum, 4);
    uint32_t ln_color = (line == cursor_row) ? 0x8B949E : 0x484F58;
    gfx_draw_string_transparent(x + 4, content_y + vis_line * ED_LINE_H, lnum, ln_color);

    if (cursor_row == line) {
        gfx_fill_rect(x + ED_GUTTER_W, content_y + vis_line * ED_LINE_H, w - ED_GUTTER_W, ED_LINE_H, 0x161B22);
    }

    for (int i = char_idx; i <= ed_content_len && vis_line < vis_lines; i++) {
        if (i == ed_cursor) {
            extern uint32_t timer_get_ms(void);
            if ((timer_get_ms() / 500) % 2 == 0) {
                int cx = content_x + col * ED_CHAR_W;
                int cy = content_y + vis_line * ED_LINE_H;
                gfx_fill_rect(cx, cy, 2, ED_LINE_H, 0xE6EDF3);
            }
        }

        if (i >= ed_content_len) break;

        /* Draw selection + find match */
        int sel_vis = 0;
        int find_vis = 0;
        if (sel_start >= 0 && i >= sel_start && i < sel_end) sel_vis = 1;
        if (ed_find_active && ed_find_pos >= 0 && i >= ed_find_pos && i < ed_find_pos + ed_find_len) find_vis = 1;

        if (ed_content[i] == '\n') {
            line++;
            col = 0;
            vis_line++;
            if (vis_line < vis_lines) {
                ed_format_number(line + 1, lnum, 4);
                ln_color = (line == cursor_row) ? 0x8B949E : 0x484F58;
                gfx_draw_string_transparent(x + 4, content_y + vis_line * ED_LINE_H, lnum, ln_color);
                if (cursor_row == line) {
                    gfx_fill_rect(x + ED_GUTTER_W, content_y + vis_line * ED_LINE_H, w - ED_GUTTER_W, ED_LINE_H, 0x161B22);
                }
            }
        } else {
            if (col >= ed_scroll_x && col < ed_scroll_x + max_chars_per_line) {
                char s[2] = {ed_content[i], 0};
                uint32_t c = 0xC9D1D9;
                uint32_t bg = 0;
                if (sel_vis) {
                    bg = 0x30363D;
                    c = 0xFFFFFF;
                } else if (find_vis) {
                    bg = 0x3D2E00;
                    c = 0xFFA657;
                }
                if (bg) {
                    gfx_fill_rect(content_x + (col - ed_scroll_x) * ED_CHAR_W, content_y + vis_line * ED_LINE_H, ED_CHAR_W, ED_LINE_H, bg);
                }
                gfx_draw_string_transparent(content_x + (col - ed_scroll_x) * ED_CHAR_W, content_y + vis_line * ED_LINE_H, s, c);
            }
            col++;
        }
    }

    /* Horizontal scrollbar */
    int hsb_y = content_y + text_h;
    int hsb_x = x + ED_GUTTER_W;
    int hsb_w = w - ED_GUTTER_W;
    gfx_fill_rect(hsb_x, hsb_y, hsb_w, hsb_h, 0x0A0E14);
    gfx_draw_hline(hsb_x, hsb_y, hsb_w, 0x21262D);
    gfx_draw_hline(hsb_x, hsb_y + hsb_h - 1, hsb_w, 0x21262D);

    int max_line_len = 0;
    int ll = 0;
    for (int i = 0; i < ed_content_len; i++) {
        if (ed_content[i] == '\n') {
            if (ll > max_line_len) max_line_len = ll;
            ll = 0;
        } else ll++;
    }
    if (ll > max_line_len) max_line_len = ll;
    int content_w_px = max_line_len * ED_CHAR_W;
    if (content_w_px < hsb_w) content_w_px = hsb_w;

    int thumb_w = (hsb_w * hsb_w) / content_w_px;
    if (thumb_w < 20) thumb_w = 20;
    int thumb_x = hsb_x + (ed_scroll_x * (hsb_w - thumb_w)) / (content_w_px > hsb_w ? content_w_px - hsb_w : 1);
    if (thumb_x < hsb_x) thumb_x = hsb_x;
    gfx_fill_rect(thumb_x, hsb_y + 1, thumb_w, hsb_h - 2, 0x30363D);

    /* Vertical scrollbar */
    int vsb_x = x + w - ED_VSB_W;
    int vsb_y = content_y;
    int vsb_h = text_h;
    gfx_fill_rect(vsb_x, vsb_y, ED_VSB_W, vsb_h, 0x0A0E14);
    gfx_draw_vline(vsb_x, vsb_y, vsb_h, 0x21262D);
    gfx_draw_vline(vsb_x + ED_VSB_W - 1, vsb_y, vsb_h, 0x21262D);

    int total_lines = ed_count_lines();
    if (total_lines < 1) total_lines = 1;
    int vsb_thumb_h = (vsb_h * vis_lines) / total_lines;
    if (vsb_thumb_h < 20) vsb_thumb_h = 20;
    int vsb_range = vsb_h - vsb_thumb_h;
    int max_scroll_y = total_lines - vis_lines;
    if (max_scroll_y < 0) max_scroll_y = 0;
    int vsb_thumb_y = vsb_y;
    if (max_scroll_y > 0) vsb_thumb_y = vsb_y + (ed_scroll_y * vsb_range) / max_scroll_y;
    gfx_fill_rect(vsb_x + 1, vsb_thumb_y, ED_VSB_W - 2, vsb_thumb_h, 0x30363D);

    /* Find bar */
    if (ed_find_active) {
        int fy = content_y + text_h + hsb_h;
        gfx_fill_rect(x, fy, w, find_bar_h, 0x161B22);
        gfx_draw_hline(x, fy, w, 0x30363D);
        gfx_draw_string_transparent(x + 10, fy + 3, "Find:", 0x8B949E);
        int fi = 0;
        while (ed_find_buf[fi] && fi < 48) fi++;
        gfx_draw_string_transparent(x + 60, fy + 3, ed_find_buf, 0xE6EDF3);
        if ((timer_get_ms() / 500) % 2 == 0) {
            int cx = x + 60 + fi * 8;
            if (cx < x + w - 20) gfx_fill_rect(cx, fy + 2, 2, 14, 0xC9D1D9);
        }
    }

    /* Bottom status bar */
    int status_y = y + h - status_h;
    gfx_fill_rect(x, status_y, w, 22, 0x161B22);
    gfx_draw_hline(x, status_y, w, 0x30363D);

    if (w > 150) {
        char pos_str[48];
        int pi = 0;
        pos_str[pi++] = 'L'; pos_str[pi++] = 'n'; pos_str[pi++] = ' ';
        char temp[8];
        itoa(cursor_row + 1, temp);
        for (int j = 0; temp[j]; j++) pos_str[pi++] = temp[j];
        pos_str[pi++] = ','; pos_str[pi++] = ' ';
        pos_str[pi++] = 'C'; pos_str[pi++] = 'o'; pos_str[pi++] = 'l'; pos_str[pi++] = ' ';
        itoa(cursor_col + 1, temp);
        for (int j = 0; temp[j]; j++) pos_str[pi++] = temp[j];
        pos_str[pi] = 0;
        gfx_draw_string_transparent(x + 10, status_y + 3, pos_str, 0x8B949E);
    }

    if (w > 300) {
        int total = ed_count_lines();
        char temp[8]; itoa(total, temp);
        char lines_str[32]; int pi = 0;
        for (int j = 0; temp[j]; j++) lines_str[pi++] = temp[j];
        lines_str[pi++] = ' '; lines_str[pi++] = 'l'; lines_str[pi++] = 'i';
        lines_str[pi++] = 'n'; lines_str[pi++] = 'e'; lines_str[pi++] = 's';
        lines_str[pi] = 0;
        gfx_draw_string_transparent(x + 180, status_y + 3, lines_str, 0x6E7681);
    }

    if (w > 450) {
        char temp[8]; itoa(ed_content_len, temp);
        char size_str[32]; int pi = 0;
        for (int j = 0; temp[j]; j++) size_str[pi++] = temp[j];
        size_str[pi++] = ' '; size_str[pi++] = 'B';
        size_str[pi] = 0;
        gfx_draw_string_transparent(x + w - 80, status_y + 3, size_str, 0x6E7681);
    }

    if (w > 550 && !ed_naming) {
        gfx_draw_string_transparent(x + w - 160, status_y + 3, "UTF-8", 0x484F58);
    }

    if (ed_naming) {
        gfx_fill_rect(x + w - 260, status_y + 1, 254, 20, 0x0D1117);
        gfx_draw_rect_outline(x + w - 260, status_y + 1, 254, 20, 1, 0x58A6FF);
        char prompt[64] = "Name: ";
        int pi = 6;
        for (int j = 0; ed_name_buf[j] && pi < 62; j++) prompt[pi++] = ed_name_buf[j];
        prompt[pi] = 0;
        gfx_draw_string_transparent(x + w - 256, status_y + 4, prompt, 0xF0F6FC);
    }

    /* Clipboard action indicator */
    if (ed_clip_action && (timer_get_ms() - ed_clip_action_time) < ED_CLIP_INDICATOR_MS) {
        const char* msg = 0;
        uint32_t col = 0;
        if (ed_clip_action == 1) { msg = "Copied!"; col = 0x3FB950; }
        else if (ed_clip_action == 2) { msg = "Cut!"; col = 0xF85149; }
        else if (ed_clip_action == 3) { msg = "Pasted!"; col = 0x58A6FF; }
        if (msg) {
            int iw = 70, ih = 22;
            int ix = x + w - iw - 10;
            int iy = status_y - ih - 4;
            gfx_fill_rect(ix, iy, iw, ih, 0x21262D);
            gfx_fill_rect(ix, iy, iw, 1, col);
            gfx_fill_rect(ix, iy + ih - 1, iw, 1, col);
            gfx_fill_rect(ix, iy, 1, ih, col);
            gfx_fill_rect(ix + iw - 1, iy, 1, ih, col);
            gfx_draw_string_transparent(ix + 8, iy + 4, msg, col);
        }
    }
}

static void ed_on_key(int id, char key_in) {
    unsigned char k = (unsigned char)key_in;
    wm_window_t* win = wm_get_window(id);
    int win_w = win ? win->w : 640;
    int vis_lines = ed_get_visible_lines(win ? win->h - WM_TITLEBAR_H : 480);

    if (ed_naming) {
        if (k == 27) { ed_naming = 0; }
        else if (k == '\b' && ed_name_len > 0) { ed_name_buf[--ed_name_len] = 0; }
        else if (k == '\n' && ed_name_len > 0) {
            ed_naming = 0;
            char old_path[128];
            fs_pwd(old_path, 128);
            fs_cd("/home/user/Documents");
            ed_do_save(ed_name_buf);
            fs_cd(old_path);
        } else if (k >= 32 && k <= 126 && ed_name_len < 62) {
            ed_name_buf[ed_name_len++] = k;
            ed_name_buf[ed_name_len] = 0;
        }
        return;
    }

    /* Ctrl+F */
    if (k == 6) { ed_find_active = !ed_find_active; if (ed_find_active) ed_find_len = 0; return; }
    
    /* Find mode input */
    if (ed_find_active) {
        if (k == 27) { ed_find_active = 0; return; }
        else if (k == '\b') {
            if (ed_find_len > 0) ed_find_buf[--ed_find_len] = 0;
            ed_find_pos = -1;
            return;
        }
        else if (k == '\n') { ed_find_next(); return; }
        else if (k >= 32 && k <= 126 && ed_find_len < 63) {
            ed_find_buf[ed_find_len++] = k;
            ed_find_buf[ed_find_len] = 0;
            ed_find_pos = -1;
            return;
        }
        return;
    }
    
    /* Ctrl+Z */

    if (k == 19) { save_dialog_open(ed_filename[0] ? ed_filename : "untitled.txt", ed_do_save); return; }

    if (k == 1) { ed_cursor = 0; ed_scroll_y = 0; ed_selection_start = -1; return; }
    if (k == 5) { ed_cursor = ed_content_len; ed_scroll_y = 0x7FFF; ed_selection_start = -1; return; }

    /* Ctrl+X — also catch via keyboard state for robustness with shift-selection */
    if (k == 24 || (keyboard_is_key_down(0x1D) && (k == 'x' || k == 'X'))) { ed_clip_cut(); return; }
    /* Ctrl+C */
    if (k == 3 || (keyboard_is_key_down(0x1D) && (k == 'c' || k == 'C'))) { ed_clip_copy(); return; }
    /* Ctrl+V */
    if (k == 22 || (keyboard_is_key_down(0x1D) && (k == 'v' || k == 'V'))) { ed_clip_paste(); return; }
    /* Ctrl+Z */
    if (k == 26) {
        /* Simple undo: restore previous content from clipboard if empty - placeholder */
        return;
    }

    if (k == '\b') {
        ed_delete_char();
    } else if (k == 127) {
        ed_delete_forward();
    } else if (k == '\n') {
        ed_insert_char('\n');
    } else if (k == '\t') {
        for (int t = 0; t < 4; t++) ed_insert_char(' ');
    } else if (k == 128) {
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int old_cursor = ed_cursor;
        if (row > 0) {
            int prev_len = ed_line_length(row - 1);
            int target_col = col < prev_len ? col : prev_len;
            ed_cursor = ed_line_start(row - 1) + target_col;
        }
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 129) {
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int old_cursor = ed_cursor;
        int total = ed_count_lines();
        if (row < total - 1) {
            int next_len = ed_line_length(row + 1);
            int target_col = col < next_len ? col : next_len;
            ed_cursor = ed_line_start(row + 1) + target_col;
        }
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 130) {
        int old_cursor = ed_cursor;
        if (ed_cursor > 0) ed_cursor--;
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 131) {
        int old_cursor = ed_cursor;
        if (ed_cursor < ed_content_len) ed_cursor++;
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 133) {
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int old_cursor = ed_cursor;
        int new_row = row - vis_lines;
        if (new_row < 0) new_row = 0;
        int new_len = ed_line_length(new_row);
        int target_col = col < new_len ? col : new_len;
        ed_cursor = ed_line_start(new_row) + target_col;
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 134) {
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int old_cursor = ed_cursor;
        int total = ed_count_lines();
        int new_row = row + vis_lines;
        if (new_row >= total) new_row = total - 1;
        int new_len = ed_line_length(new_row);
        int target_col = col < new_len ? col : new_len;
        ed_cursor = ed_line_start(new_row) + target_col;
        if (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36)) {
            if (ed_selection_start < 0) ed_selection_start = old_cursor;
        } else {
            ed_selection_start = -1;
        }
    } else if (k == 3 || k == 22 || k == 24) {
        /* Handled above */
    } else if (k >= 32 && k <= 126) {
        if (ed_selection_start >= 0 && ed_selection_start != ed_cursor) {
            int start = ed_selection_start;
            int end = ed_cursor;
            if (start > end) { int t = start; start = end; end = t; }
            for (int i = end; i < ed_content_len; i++) ed_content[start + i - end] = ed_content[i];
            ed_content_len -= (end - start);
            ed_cursor = start;
            ed_selection_start = -1;
        } else {
            ed_selection_start = -1;
        }
        ed_insert_char(k);
    }
}

static void ed_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

static void ed_on_mouse(int id, int mx, int my, int mb) {
    (void)id;
    wm_window_t* win = wm_get_window(ed_win_id);
    if (!win) return;

    int text_h = win->h - WM_TITLEBAR_H - 22 - 10 - (ed_find_active ? 22 : 0); /* minus status, hsb, find */
    int hsb_y = text_h;
    int hsb_x = ED_GUTTER_W;
    int hsb_w = win->w - ED_GUTTER_W;
    int vsb_x = win->w - ED_VSB_W;
    int vsb_y = ED_TOOLBAR_H;
    int vsb_h = text_h;
    int cx = mx;
    int cy = my;

    /* Handle vertical scrollbar drag */
    if (ed_drag_vsb) {
        if (mb & 1) {
            int total_lines = ed_count_lines();
            if (total_lines < 1) total_lines = 1;
            int vis_lines = ed_get_visible_lines(win->h - WM_TITLEBAR_H - 10 - (ed_find_active ? 22 : 0));
            int vsb_thumb_h = (vsb_h * vis_lines) / total_lines;
            if (vsb_thumb_h < 20) vsb_thumb_h = 20;
            int vsb_range = vsb_h - vsb_thumb_h;
            int max_scroll_y = total_lines - vis_lines;
            if (max_scroll_y < 0) max_scroll_y = 0;
            if (vsb_range > 0) {
                ed_scroll_y = ((cy - vsb_thumb_h / 2) * max_scroll_y) / vsb_range;
                if (ed_scroll_y < 0) ed_scroll_y = 0;
                if (ed_scroll_y > max_scroll_y) ed_scroll_y = max_scroll_y;
            }
            return;
        } else {
            ed_drag_vsb = 0;
        }
    }

    /* Handle horizontal scrollbar drag */
    if (ed_drag_hsb) {
        if (mb & 1) {
            int max_line_len = 0;
            int ll = 0;
            for (int i = 0; i < ed_content_len; i++) {
                if (ed_content[i] == '\n') {
                    if (ll > max_line_len) max_line_len = ll;
                    ll = 0;
                } else ll++;
            }
            if (ll > max_line_len) max_line_len = ll;
            int content_w_px = max_line_len * ED_CHAR_W;
            if (content_w_px < hsb_w) content_w_px = hsb_w;

            int thumb_w = (hsb_w * hsb_w) / content_w_px;
            if (thumb_w < 20) thumb_w = 20;
            int max_scroll = content_w_px - hsb_w;
            if (max_scroll < 0) max_scroll = 0;
            int range = hsb_w - thumb_w;
            if (range <= 0) ed_scroll_x = 0;
            else ed_scroll_x = ((cx - hsb_x - thumb_w / 2) * max_scroll) / range;
            if (ed_scroll_x < 0) ed_scroll_x = 0;
            if (ed_scroll_x > max_scroll) ed_scroll_x = max_scroll;
            return;
        } else {
            ed_drag_hsb = 0;
        }
    }

    /* Check vertical scrollbar click */
    if (cx >= vsb_x && cx <= vsb_x + ED_VSB_W && cy >= ED_TOOLBAR_H && cy <= text_h) {
        int total_lines = ed_count_lines();
        if (total_lines < 1) total_lines = 1;
        int vis_lines = ed_get_visible_lines(win->h - WM_TITLEBAR_H - 10 - (ed_find_active ? 22 : 0));
        int vsb_thumb_h = (vsb_h * vis_lines) / total_lines;
        if (vsb_thumb_h < 20) vsb_thumb_h = 20;
        int max_scroll_y = total_lines - vis_lines;
        if (max_scroll_y < 0) max_scroll_y = 0;

        int thumb_y = vsb_y;
        if (max_scroll_y > 0) thumb_y = vsb_y + (ed_scroll_y * (vsb_h - vsb_thumb_h)) / max_scroll_y;
        int thumb_end = thumb_y + vsb_thumb_h;

        if (cy >= thumb_y && cy <= thumb_end) {
            ed_drag_vsb = 1;
        } else {
            /* Click on track - jump */
            if (cy < thumb_y) ed_scroll_y = 0;
            else ed_scroll_y = max_scroll_y;
        }
        ed_selection_start = -1;
        ed_drag_selecting = 0;
        return;
    }

    /* Check horizontal scrollbar click */
    if (cy >= text_h && cy <= text_h + 10 && cx >= hsb_x && cx <= hsb_x + hsb_w) {
        int max_line_len = 0;
        int ll = 0;
        for (int i = 0; i < ed_content_len; i++) {
            if (ed_content[i] == '\n') {
                if (ll > max_line_len) max_line_len = ll;
                ll = 0;
            } else ll++;
        }
        if (ll > max_line_len) max_line_len = ll;
        int content_w_px = max_line_len * ED_CHAR_W;
        if (content_w_px < hsb_w) content_w_px = hsb_w;

        int thumb_w = (hsb_w * hsb_w) / content_w_px;
        if (thumb_w < 20) thumb_w = 20;
        int max_scroll = content_w_px - hsb_w;
        if (max_scroll < 0) max_scroll = 0;
        int range = hsb_w - thumb_w;

        int thumb_x = hsb_x;
        if (range > 0) thumb_x = hsb_x + (ed_scroll_x * range) / max_scroll;
        int thumb_end = thumb_x + thumb_w;

        if (cx >= thumb_x && cx <= thumb_end) {
            ed_drag_hsb = 1;
        } else {
            /* Click on track - jump */
            if (cx < thumb_x) ed_scroll_x = 0;
            else ed_scroll_x = max_scroll;
        }
        ed_selection_start = -1;
        ed_drag_selecting = 0;
        return;
    }

    if (cy < ED_TOOLBAR_H || cy > text_h + 10) return;
    int rel_x = cx - ED_GUTTER_W;
    int rel_y = cy;

    if (mb & 1 || ed_drag_selecting) {
        int col = rel_x / ED_CHAR_W + ed_scroll_x;
        int row = rel_y / ED_LINE_H + ed_scroll_y;

        /* Find line start for this row */
        int pos = 0, l = 0;
        while (pos < ed_content_len && l < row) {
            if (ed_content[pos] == '\n') l++;
            pos++;
        }
        /* Clamp col to line length */
        int line_len = 0;
        while (pos + line_len < ed_content_len && ed_content[pos + line_len] != '\n') line_len++;
        if (col > line_len) col = line_len;
        ed_cursor = pos + col;

        if (mb & 1) {
            if (!ed_drag_selecting) {
                ed_press_x = mx;
                ed_press_y = my;
                ed_selection_start = ed_cursor;
                ed_drag_selecting = 1;
            }
        } else {
            int was_dragging = ed_drag_selecting;
            ed_drag_selecting = 0;
            if (!was_dragging) {
                ed_selection_start = -1;
            } else {
                int dx = mx - ed_press_x;
                int dy = my - ed_press_y;
                if (dx*dx + dy*dy < 25) {
                    ed_selection_start = -1;
                }
            }
        }
    }
}

static void ed_btn_new(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    text_editor_new("untitled.txt");
}
static void ed_btn_save(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    save_dialog_open(ed_filename[0] ? ed_filename : "untitled.txt", ed_do_save);
}
static void ed_btn_cut(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    ed_clip_cut();
}
static void ed_btn_copy(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    ed_clip_copy();
}
static void ed_btn_paste(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    ed_clip_paste();
}
static void ed_btn_find(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    ed_find_active = 1;
    ed_find_len = 0;
    ed_find_pos = -1;
}

void text_editor_open(const char* filename) {
    int i = 0;
    while (filename[i] && i < 63) { ed_filename[i] = filename[i]; i++; }
    ed_filename[i] = 0;

    ed_content_len = 0;
    ed_cursor = 0;
    ed_scroll_y = 0;
    ed_dirty = 0;
    ed_selection_start = -1;
    ed_clip_len = 0;
    ed_find_active = 0;
    ed_find_len = 0;

    int fd = fs_open(filename, 0);
    if (fd >= 0) {
        ed_content_len = fs_read(fd, ed_content, ED_MAX_SIZE);
        if (ed_content_len < 0) ed_content_len = 0;
        fs_close(fd);
    }
    ed_content[ed_content_len] = 0;

    char title[80] = "Edit: ";
    int ti = 6;
    for (int j = 0; ed_filename[j] && ti < 78; j++) title[ti++] = ed_filename[j];
    title[ti] = 0;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 680, win_h = 500;
    ed_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                                title, 0x58A6FF, ed_on_render, ed_on_key, ed_on_resize);
    if (ed_win_id >= 0) {
        wm_set_mouse_handler(ed_win_id, ed_on_mouse);
        wm_clear_buttons(ed_win_id);
        int bx = 6, by = 4, bw = 56, bh = 20, gap = 4;
        wm_add_button(ed_win_id, BTN_NEW, bx, by, bw, bh, "New", 0x21262D, 0xE6EDF3, ed_btn_new);
        bx += bw + gap;
        wm_add_button(ed_win_id, BTN_SAVE, bx, by, bw, bh, "Save", 0x21262D, 0xE6EDF3, ed_btn_save);
        bx += bw + gap;
        bx += 16; /* separator */
        wm_add_button(ed_win_id, BTN_CUT, bx, by, bw, bh, "Cut", 0x21262D, 0xE6EDF3, ed_btn_cut);
        bx += bw + gap;
        wm_add_button(ed_win_id, BTN_COPY, bx, by, bw, bh, "Copy", 0x21262D, 0xE6EDF3, ed_btn_copy);
        bx += bw + gap;
        wm_add_button(ed_win_id, BTN_PASTE, bx, by, bw, bh, "Paste", 0x21262D, 0xE6EDF3, ed_btn_paste);
        bx += bw + gap;
        bx += 16;
        wm_add_button(ed_win_id, BTN_FIND, bx, by, bw, bh, "Find", 0x21262D, 0xE6EDF3, ed_btn_find);
    }
}

void text_editor_new(const char* filename) {
    int fd = fs_create(filename);
    if (fd >= 0) fs_close(fd);
    text_editor_open(filename);
}
void text_editor(void) { text_editor_new("untitled.txt"); }

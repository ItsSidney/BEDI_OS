#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "filesystem/filesystem.h"
#include "commands/commands.h"
#include "kernel/time/timer.h"

#define BDIM_MAX_LINES      1024
#define BDIM_MAX_COL        512
#define BDIM_MAX_NAME       128
#define BDIM_GUTTER_W       48
#define BDIM_LINE_H         18
#define BDIM_CHAR_W         8
#define BDIM_STATUS_H       24
#define BDIM_CMD_H          24
#define BDIM_TOOLBAR_H      28
#define BDIM_CLIP_LEN       4096

/* Syntax highlighting */
#define SH_KEYWORD  1
#define SH_STRING   2
#define SH_COMMENT  3
#define SH_NUMBER   4
#define SH_PREPROC  5
#define SH_NORMAL   0
#define SH_OPERATOR 6

#define CLR_SH_KEYWORD   0x569CD6
#define CLR_SH_STRING    0xCE9178
#define CLR_SH_COMMENT   0x6A9955
#define CLR_SH_NUMBER    0xB5CEA8
#define CLR_SH_PREPROC   0xC586C0
#define CLR_SH_NORMAL    0xD4D4D4
#define CLR_SH_OPERATOR  0xD4D4D4

#define MODE_NORMAL  0
#define MODE_INSERT  1
#define MODE_VISUAL  2
#define MODE_COMMAND 3

#define CLR_BG          0x1E1E1E
#define CLR_GUTTER      0x252526
#define CLR_GUTTER_TXT  0x858585
#define CLR_TEXT        0xD4D4D4
#define CLR_CURSOR      0xFFFFFF
#define CLR_SELECTION   0x264F78
#define CLR_STATUS_N    0x3281C6
#define CLR_STATUS_I    0x769A38
#define CLR_STATUS_V    0xB35E0E
#define CLR_STATUS_C    0x666666
#define CLR_LINENUM     0x858585
#define CLR_LINENUM_ACT 0xFFFFFF
#define CLR_CMDBG       0x1E1E1E
#define CLR_CMDTXT      0xCCCCCC
#define CLR_DIRTY       0xF0883E
#define CLR_SEARCH      0x3D2E00
#define CLR_SEARCH_TXT  0xFFA657

typedef struct {
    char lines[BDIM_MAX_LINES][BDIM_MAX_COL];
    int  line_len[BDIM_MAX_LINES];
    int  line_count;

    int  cx, cy;
    int  scroll_x, scroll_y;

    int  mode;

    int  vis_start_line;
    int  vis_start_col;

    char clipboard[BDIM_CLIP_LEN];
    int  clip_len;

    char cmd_buf[128];
    int  cmd_len;
    int  cmd_active;
    char cmd_msg[128];
    int  cmd_msg_timer;

    char filename[BDIM_MAX_NAME];
    int  dirty;
    int  open;

    char search[64];
    int  search_len;
    int  search_pos;
    int  search_active;

    int  win_id;
    int  mouse_selecting;
    int  mouse_down_x, mouse_down_y;
    int  last_paste_timer;

    int  quit_on_clean;
    int  show_help;

    int  prev_mode;
    int  in_block_comment;
} bdim_state_t;

static bdim_state_t bd;

static const char* c_keywords[] = {
    "auto","break","case","char","const","continue","default","do",
    "double","else","enum","extern","float","for","goto","if",
    "inline","int","long","register","return","short","signed",
    "sizeof","static","struct","switch","typedef","union","unsigned",
    "void","volatile","while",
    "size_t","uint8_t","uint16_t","uint32_t","uint64_t",
    "int8_t","int16_t","int32_t","int64_t",
    "uintptr_t","intptr_t","bool","true","false","NULL",
    0
};

static int sh_is_keyword(const char* s, int len) {
    for (int i = 0; c_keywords[i]; i++) {
        const char* kw = c_keywords[i];
        int j = 0;
        while (kw[j] && j < len && kw[j] == s[j]) j++;
        if (kw[j] == 0 && j == len) return 1;
    }
    return 0;
}

static int sh_is_ident_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static int sh_is_op_char(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '=' || c == '!' || c == '<' || c == '>' || c == '&' ||
           c == '|' || c == '^' || c == '~' || c == '?' || c == ':';
}

static void sh_tokenize_line(const char* line, int len, int* in_block, int* out_tok, int* out_len) {
    int i = 0;
    while (i < len) {
        /* block comment active */
        if (*in_block) {
            int start = i;
            while (i < len - 1) {
                if (line[i] == '*' && line[i+1] == '/') {
                    i += 2;
                    *in_block = 0;
                    break;
                }
                i++;
            }
            if (*in_block) i = len;
            for (int j = start; j < i && j < *out_len; j++) { out_tok[j] = SH_COMMENT; }
            continue;
        }

        /* preprocessor */
        if (line[i] == '#' && (i == 0 || line[i-1] == '\n')) {
            int start = i;
            while (i < len && line[i] != '\n') i++;
            i = len; /* highlight rest of line */
            for (int j = start; j < i && j < *out_len; j++) { out_tok[j] = SH_PREPROC; }
            continue;
        }

        /* single-line comment */
        if (i < len - 1 && line[i] == '/' && line[i+1] == '/') {
            for (int j = i; j < len && j < *out_len; j++) out_tok[j] = SH_COMMENT;
            i = len;
            continue;
        }

        /* block comment start */
        if (i < len - 1 && line[i] == '/' && line[i+1] == '*') {
            int start = i;
            *in_block = 1;
            i += 2;
            while (i < len - 1) {
                if (line[i] == '*' && line[i+1] == '/') {
                    i += 2;
                    *in_block = 0;
                    break;
                }
                i++;
            }
            if (*in_block) i = len;
            for (int j = start; j < i && j < *out_len; j++) out_tok[j] = SH_COMMENT;
            continue;
        }

        /* string literal */
        if (line[i] == '"') {
            int start = i;
            i++;
            while (i < len && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            for (int j = start; j < i && j < *out_len; j++) out_tok[j] = SH_STRING;
            continue;
        }

        /* char literal */
        if (line[i] == '\'') {
            int start = i;
            i++;
            while (i < len && line[i] != '\'') {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++;
            for (int j = start; j < i && j < *out_len; j++) out_tok[j] = SH_STRING;
            continue;
        }

        /* number */
        if ((line[i] >= '0' && line[i] <= '9') ||
            (line[i] == '.' && i + 1 < len && line[i+1] >= '0' && line[i+1] <= '9')) {
            int start = i;
            if (line[i] == '0' && i + 1 < len && (line[i+1] == 'x' || line[i+1] == 'X')) {
                i += 2;
                while (i < len && ((line[i] >= '0' && line[i] <= '9') ||
                       (line[i] >= 'a' && line[i] <= 'f') ||
                       (line[i] >= 'A' && line[i] <= 'F') || line[i] == '.')) i++;
            } else {
                while (i < len && ((line[i] >= '0' && line[i] <= '9') || line[i] == '.' ||
                       line[i] == 'x' || line[i] == 'X' ||
                       (line[i] >= 'a' && line[i] <= 'f') ||
                       (line[i] >= 'A' && line[i] <= 'F') ||
                       line[i] == 'u' || line[i] == 'U' ||
                       line[i] == 'l' || line[i] == 'L' ||
                       line[i] == 'f' || line[i] == 'F')) i++;
            }
            for (int j = start; j < i && j < *out_len; j++) out_tok[j] = SH_NUMBER;
            continue;
        }

        /* identifier/keyword */
        if (sh_is_ident_char(line[i])) {
            int start = i;
            while (i < len && sh_is_ident_char(line[i])) i++;
            int is_kw = sh_is_keyword(line + start, i - start);
            int tok = is_kw ? SH_KEYWORD : SH_NORMAL;
            for (int j = start; j < i && j < *out_len; j++) out_tok[j] = tok;
            continue;
        }

        /* operator */
        if (sh_is_op_char(line[i])) {
            out_tok[i] = SH_OPERATOR;
            i++;
            continue;
        }

        i++;
    }
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }
static int min_int(int a, int b) { return a < b ? a : b; }
static int max_int(int a, int b) { return a > b ? a : b; }

static void bd_line_init(int n) {
    bd.line_len[n] = 0;
    bd.lines[n][0] = 0;
}

static void bd_ensure_line(int n) {
    while (bd.line_count <= n) {
        bd_line_init(bd.line_count);
        bd.line_count++;
    }
}

static void bd_insert_char_at(int line, int col, char c) {
    bd_ensure_line(line);
    if (bd.line_len[line] >= BDIM_MAX_COL - 1) return;
    for (int i = bd.line_len[line]; i > col; i--)
        bd.lines[line][i] = bd.lines[line][i-1];
    bd.lines[line][col] = c;
    bd.line_len[line]++;
    bd.lines[line][bd.line_len[line]] = 0;
}

static void bd_delete_char_at(int line, int col) {
    if (col < 0 || col >= bd.line_len[line]) return;
    for (int i = col; i < bd.line_len[line] - 1; i++)
        bd.lines[line][i] = bd.lines[line][i+1];
    bd.line_len[line]--;
    bd.lines[line][bd.line_len[line]] = 0;
}

static void bd_delete_line(int line) {
    if (line < 0 || line >= bd.line_count) return;
    for (int i = line; i < bd.line_count - 1; i++) {
        bd.line_len[i] = bd.line_len[i+1];
        for (int j = 0; j < bd.line_len[i]; j++)
            bd.lines[i][j] = bd.lines[i+1][j];
        bd.lines[i][bd.line_len[i]] = 0;
    }
    bd.line_count--;
    if (bd.line_count < 1) { bd.line_count = 1; bd_line_init(0); }
}

static void bd_insert_line(int line) {
    bd_ensure_line(bd.line_count);
    for (int i = bd.line_count; i > line; i--) {
        bd.line_len[i] = bd.line_len[i-1];
        for (int j = 0; j < bd.line_len[i-1]; j++)
            bd.lines[i][j] = bd.lines[i-1][j];
        bd.lines[i][bd.line_len[i]] = 0;
    }
    bd.line_len[line] = 0;
    bd.lines[line][0] = 0;
    bd.line_count++;
}

static void bd_split_line(int line, int col) {
    bd_ensure_line(bd.line_count);
    for (int i = bd.line_count; i > line + 1; i--) {
        bd.line_len[i] = bd.line_len[i-1];
        for (int j = 0; j < bd.line_len[i-1]; j++)
            bd.lines[i][j] = bd.lines[i-1][j];
        bd.lines[i][bd.line_len[i]] = 0;
    }
    int new_len = bd.line_len[line] - col;
    for (int j = 0; j < new_len; j++)
        bd.lines[line+1][j] = bd.lines[line][col + j];
    bd.line_len[line+1] = new_len;
    bd.lines[line+1][new_len] = 0;
    bd.line_len[line] = col;
    bd.lines[line][col] = 0;
    bd.line_count++;
}

static void bd_join_lines(int line) {
    if (line < 0 || line >= bd.line_count - 1) return;
    int old_len = bd.line_len[line];
    int join_len = bd.line_len[line+1];
    if (old_len + join_len >= BDIM_MAX_COL - 1) join_len = BDIM_MAX_COL - 1 - old_len;
    for (int j = 0; j < join_len; j++)
        bd.lines[line][old_len + j] = bd.lines[line+1][j];
    bd.line_len[line] = old_len + join_len;
    bd.lines[line][bd.line_len[line]] = 0;
    bd_delete_line(line+1);
}

static void bd_clip_copy(void) {
    if (!bd.open) return;
    if (bd.mode == MODE_VISUAL) {
        int sl = bd.vis_start_line, sc = bd.vis_start_col;
        int el = bd.cy, ec = bd.cx;
        if (sl > el || (sl == el && sc > ec)) {
            int t = sl; sl = el; el = t;
            t = sc; sc = ec; ec = t;
        }
        bd.clip_len = 0;
        for (int l = sl; l <= el; l++) {
            int start = (l == sl) ? sc : 0;
            int end = (l == el) ? ec : bd.line_len[l];
            for (int c = start; c < end && bd.clip_len < BDIM_CLIP_LEN - 1; c++)
                bd.clipboard[bd.clip_len++] = bd.lines[l][c];
            if (l < el && bd.clip_len < BDIM_CLIP_LEN - 1)
                bd.clipboard[bd.clip_len++] = '\n';
        }
        bd.clipboard[bd.clip_len] = 0;
    }
}

static void bd_clip_cut(void) {
    bd_clip_copy();
    if (bd.mode == MODE_VISUAL && bd.clip_len > 0) {
        int sl = bd.vis_start_line, sc = bd.vis_start_col;
        int el = bd.cy, ec = bd.cx;
        if (sl > el || (sl == el && sc > ec)) {
            int t = sl; sl = el; el = t;
            t = sc; sc = ec; ec = t;
        }
        if (sl == el) {
            for (int c = sc; c < ec; c++) bd_delete_char_at(sl, sc);
        } else {
            bd.line_len[sl] = sc;
            bd.lines[sl][sc] = 0;
            int tail_len = bd.line_len[el] - ec;
            for (int c = 0; c < tail_len; c++)
                bd.lines[sl][sc + c] = bd.lines[el][ec + c];
            bd.line_len[sl] = sc + tail_len;
            bd.lines[sl][bd.line_len[sl]] = 0;
            for (int l = el; l > sl + 1; l--) bd_delete_line(l);
            bd_delete_line(sl + 1);
        }
        bd.cx = sc; bd.cy = sl;
        bd.mode = MODE_NORMAL;
        bd.dirty = 1;
    }
}

static void bd_clip_paste(void) {
    if (!bd.open || bd.clip_len == 0) return;
    if (bd.mode == MODE_VISUAL) {
        bd_clip_cut();
    }
    int lines_in_clip = 0;
    for (int i = 0; i < bd.clip_len; i++)
        if (bd.clipboard[i] == '\n') lines_in_clip++;
    if (lines_in_clip == 0) {
        for (int i = 0; i < bd.clip_len; i++)
            bd_insert_char_at(bd.cy, bd.cx + i, bd.clipboard[i]);
        bd.cx += bd.clip_len;
    } else {
        int orig_cx = bd.cx;
        int line = bd.cy;
        int col = bd.cx;
        int clip_i = 0;
        while (clip_i < bd.clip_len) {
            bd_ensure_line(line);
            if (bd.clipboard[clip_i] == '\n') {
                bd_split_line(line, col);
                line++; col = 0;
                clip_i++;
            } else {
                bd_insert_char_at(line, col, bd.clipboard[clip_i]);
                col++; clip_i++;
            }
        }
        bd.cx = col; bd.cy = line;
    }
    bd.dirty = 1;
    bd.last_paste_timer = 1;
}

static void bd_cursor_up(void) {
    if (bd.cy > 0) {
        bd.cy--;
        if (bd.cx > bd.line_len[bd.cy]) bd.cx = bd.line_len[bd.cy];
    }
}

static void bd_cursor_down(void) {
    if (bd.cy < bd.line_count - 1) {
        bd.cy++;
        if (bd.cx > bd.line_len[bd.cy]) bd.cx = bd.line_len[bd.cy];
    }
}

static void bd_cursor_left(void) {
    if (bd.cx > 0) bd.cx--;
    else if (bd.cy > 0) { bd.cy--; bd.cx = bd.line_len[bd.cy]; }
}

static void bd_cursor_right(void) {
    if (bd.cx < bd.line_len[bd.cy]) bd.cx++;
    else if (bd.cy < bd.line_count - 1) { bd.cy++; bd.cx = 0; }
}

static void bd_cursor_word_end(void) {
    if (bd.cx >= bd.line_len[bd.cy]) {
        bd_cursor_right(); return;
    }
    while (bd.cx < bd.line_len[bd.cy] && bd.lines[bd.cy][bd.cx] == ' ') bd.cx++;
    while (bd.cx < bd.line_len[bd.cy] && bd.lines[bd.cy][bd.cx] != ' ') bd.cx++;
}

static void bd_cursor_word_start(void) {
    if (bd.cx <= 0) {
        bd_cursor_left(); return;
    }
    bd.cx--;
    while (bd.cx > 0 && bd.lines[bd.cy][bd.cx] == ' ') bd.cx--;
    while (bd.cx > 0 && bd.lines[bd.cy][bd.cx-1] != ' ') bd.cx--;
}

static void bd_cursor_line_start(void) { bd.cx = 0; }
static void bd_cursor_line_end(void) { bd.cx = bd.line_len[bd.cy]; }
static void bd_cursor_file_start(void) { bd.cy = 0; bd.cx = 0; bd.scroll_y = 0; }
static void bd_cursor_file_end(void) {
    bd.cy = bd.line_count - 1;
    bd_ensure_line(bd.cy);
    bd.cx = bd.line_len[bd.cy];
    bd.scroll_y = max_int(0, bd.cy - 20);
}

static void bd_page_up(void) {
    for (int i = 0; i < 20; i++) bd_cursor_up();
}

static void bd_page_down(void) {
    for (int i = 0; i < 20; i++) bd_cursor_down();
}

static void bd_delete_line_op(void) {
    bd.clip_len = 0;
    for (int i = 0; i < bd.line_len[bd.cy]; i++)
        bd.clipboard[bd.clip_len++] = bd.lines[bd.cy][i];
    bd.clipboard[bd.clip_len] = 0;
    bd_delete_line(bd.cy);
    if (bd.cy >= bd.line_count) bd.cy = bd.line_count - 1;
    bd.cx = 0;
    bd.dirty = 1;
}

static void bd_yank_line(void) {
    bd.clip_len = 0;
    for (int i = 0; i < bd.line_len[bd.cy]; i++)
        bd.clipboard[bd.clip_len++] = bd.lines[bd.cy][i];
    bd.clipboard[bd.clip_len] = 0;
}

static void bd_paste_line(void) {
    if (bd.clip_len == 0) return;
    bd_ensure_line(bd.line_count);
    bd_insert_line(bd.cy + 1);
    int new_line = bd.cy + 1;
    for (int i = 0; i < bd.clip_len && i < BDIM_MAX_COL - 1; i++)
        bd.lines[new_line][i] = bd.clipboard[i];
    bd.line_len[new_line] = bd.clip_len;
    bd.lines[new_line][bd.line_len[new_line]] = 0;
    bd.cy = new_line;
    bd.cx = 0;
    bd.dirty = 1;
}

static void bd_indent_line(void) {
    for (int i = 0; i < 4; i++) bd_insert_char_at(bd.cy, bd.cx++, ' ');
}

static void bd_set_msg(const char* msg) {
    int i = 0;
    while (msg[i] && i < 127) { bd.cmd_msg[i] = msg[i]; i++; }
    bd.cmd_msg[i] = 0;
    bd.cmd_msg_timer = 150;
}

static void bd_save(void) {
    if (!bd.filename[0]) {
        bd_set_msg("No filename");
        return;
    }
    fs_delete(bd.filename);
    int fd = fs_create(bd.filename);
    if (fd < 0) { bd_set_msg("Save failed"); return; }
    for (int i = 0; i < bd.line_count; i++) {
        fs_write(fd, bd.lines[i], bd.line_len[i]);
        if (i < bd.line_count - 1) fs_write(fd, "\n", 1);
    }
    fs_close(fd);
    bd.dirty = 0;
    bd_set_msg("Saved");
}

static void bd_load(const char* name) {
    bd.line_count = 0;
    bd.cx = 0; bd.cy = 0;
    bd.scroll_x = 0; bd.scroll_y = 0;
    bd.dirty = 0;
    bd.clip_len = 0;
    bd.mode = MODE_NORMAL;
    bd.open = 1;
    bd.search_active = 0;
    bd.search_len = 0;
    bd.cmd_active = 0;
    bd.cmd_len = 0;
    bd.quit_on_clean = 0;
    bd.show_help = 0;
    bd.cmd_msg[0] = 0;
    bd.cmd_msg_timer = 0;
    bd.mouse_selecting = 0;
    bd.in_block_comment = 0;

    int i = 0;
    while (name[i] && i < BDIM_MAX_NAME - 1) { bd.filename[i] = name[i]; i++; }
    bd.filename[i] = 0;

    bd_ensure_line(0);

    int fd = fs_open(name, 0);
    if (fd >= 0) {
        char ch;
        int line = 0, col = 0;
        bd_ensure_line(line);
        while (fs_read(fd, &ch, 1) == 1 && line < BDIM_MAX_LINES) {
            if (ch == '\n') {
                bd.lines[line][col] = 0;
                bd.line_len[line] = col;
                line++; col = 0;
                bd_ensure_line(line);
            } else if (col < BDIM_MAX_COL - 1) {
                bd.lines[line][col++] = ch;
            }
        }
        if (col > 0 || line > 0) {
            bd.lines[line][col] = 0;
            bd.line_len[line] = col;
        }
        line++;
        bd.line_count = line;
        fs_close(fd);
        bd_set_msg("File loaded");
    } else {
        bd_set_msg("New file");
    }
    if (bd.line_count < 1) { bd.line_count = 1; bd_line_init(0); }
}

static void bd_render(void) {
    wm_window_t* win = wm_get_window(bd.win_id);
    if (!win) return;
    int x = win->x, y = win->y + WM_TITLEBAR_H;
    int w = win->w, h = win->h - WM_TITLEBAR_H;

    int text_h = h - BDIM_STATUS_H;
    int vis_lines = text_h / BDIM_LINE_H;
    if (vis_lines < 1) vis_lines = 1;

    if (bd.cy < bd.scroll_y) bd.scroll_y = bd.cy;
    if (bd.cy >= bd.scroll_y + vis_lines) bd.scroll_y = bd.cy - vis_lines + 1;
    if (bd.scroll_y < 0) bd.scroll_y = 0;

    int max_cols = (w - BDIM_GUTTER_W - 10) / BDIM_CHAR_W;
    if (max_cols < 1) max_cols = 1;
    if (bd.cx < bd.scroll_x) bd.scroll_x = bd.cx;
    if (bd.cx >= bd.scroll_x + max_cols) bd.scroll_x = bd.cx - max_cols + 1;
    if (bd.scroll_x < 0) bd.scroll_x = 0;

    gfx_fill_rect(x, y, w, h, CLR_BG);

    gfx_fill_rect(x, y, BDIM_GUTTER_W, text_h, CLR_GUTTER);
    gfx_draw_vline(x + BDIM_GUTTER_W, y, text_h, 0x333333);

    int sel_s = -1, sel_e = -1, sel_sl = -1, sel_el = -1;
    if (bd.mode == MODE_VISUAL) {
        sel_sl = bd.vis_start_line; sel_s = bd.vis_start_col;
        sel_el = bd.cy; sel_e = bd.cx;
        if (sel_sl > sel_el || (sel_sl == sel_el && sel_s > sel_e)) {
            int t = sel_sl; sel_sl = sel_el; sel_el = t;
            t = sel_s; sel_s = sel_e; sel_e = t;
        }
    }

    char lnum[12];
    for (int v = 0; v < vis_lines; v++) {
        int ly = y + v * BDIM_LINE_H;
        int line_idx = bd.scroll_y + v;
        if (line_idx >= bd.line_count) break;

        if (line_idx == bd.cy) {
            gfx_fill_rect(x + BDIM_GUTTER_W, ly, w - BDIM_GUTTER_W, BDIM_LINE_H, 0x2A2D2E);
        }

        int lnum_w = BDIM_GUTTER_W - 12;
        int num = line_idx + 1;
        int ni = 0;
        if (num == 0) { lnum[ni++] = '0'; }
        else {
            int nt = num;
            while (nt > 0) { ni++; nt /= 10; }
            lnum[ni] = 0;
            nt = num;
            for (int j = ni - 1; j >= 0; j--) { lnum[j] = '0' + (nt % 10); nt /= 10; }
        }
        int lnum_len = ni;
        int pad = (lnum_w / BDIM_CHAR_W) - lnum_len;
        if (pad < 0) pad = 0;
        int lx = x + 6 + pad * BDIM_CHAR_W;
        uint32_t lc = (line_idx == bd.cy) ? CLR_LINENUM_ACT : CLR_LINENUM;
        gfx_draw_string_transparent(lx, ly + 2, lnum, lc);

        int sh_tokens[BDIM_MAX_COL];
        int sh_len = bd.line_len[line_idx];
        if (sh_len > BDIM_MAX_COL) sh_len = BDIM_MAX_COL;
        for (int ti = 0; ti < sh_len; ti++) sh_tokens[ti] = SH_NORMAL;
        sh_tokenize_line(bd.lines[line_idx], sh_len, &bd.in_block_comment, sh_tokens, &sh_len);

        for (int c = bd.scroll_x; c < bd.line_len[line_idx] && c < bd.scroll_x + max_cols; c++) {
            int cx = x + BDIM_GUTTER_W + 4 + (c - bd.scroll_x) * BDIM_CHAR_W;
            int in_sel = 0;
            if (sel_sl >= 0 && line_idx >= sel_sl && line_idx <= sel_el) {
                if (line_idx == sel_sl && line_idx == sel_el) {
                    if (c >= sel_s && c < sel_e) in_sel = 1;
                } else if (line_idx == sel_sl && c >= sel_s) in_sel = 1;
                else if (line_idx == sel_el && c < sel_e) in_sel = 1;
                else if (line_idx > sel_sl && line_idx < sel_el) in_sel = 1;
            }
            int in_search = 0;
            if (bd.search_active && bd.search_len > 0) {
                if (c + bd.search_len <= bd.line_len[line_idx]) {
                    int match = 1;
                    for (int si = 0; si < bd.search_len; si++)
                        if (bd.lines[line_idx][c + si] != bd.search[si]) { match = 0; break; }
                    if (match) in_search = 1;
                }
            }
            uint32_t color;
            if (in_sel) color = 0xFFFFFF;
            else if (in_search) color = CLR_SEARCH_TXT;
            else {
                int tok = (c < sh_len) ? sh_tokens[c] : SH_NORMAL;
                switch (tok) {
                    case SH_KEYWORD:  color = CLR_SH_KEYWORD; break;
                    case SH_STRING:   color = CLR_SH_STRING; break;
                    case SH_COMMENT:  color = CLR_SH_COMMENT; break;
                    case SH_NUMBER:   color = CLR_SH_NUMBER; break;
                    case SH_PREPROC:  color = CLR_SH_PREPROC; break;
                    default:          color = CLR_SH_NORMAL; break;
                }
            }
            if (in_sel) gfx_fill_rect(cx, ly, BDIM_CHAR_W, BDIM_LINE_H, CLR_SELECTION);
            else if (in_search) gfx_fill_rect(cx, ly, BDIM_CHAR_W * bd.search_len, BDIM_LINE_H, CLR_SEARCH);

            char ch[2] = {bd.lines[line_idx][c], 0};
            gfx_draw_char_transparent(cx, ly + 2, ch[0], color);
        }
    }

    if (bd.mode != MODE_VISUAL) {
        int cvx = x + BDIM_GUTTER_W + 4 + (bd.cx - bd.scroll_x) * BDIM_CHAR_W;
        int cvy = y + (bd.cy - bd.scroll_y) * BDIM_LINE_H;
        if (bd.cy >= bd.scroll_y && bd.cy < bd.scroll_y + vis_lines) {
            if (bd.mode == MODE_INSERT && (timer_get_ms() / 500) % 2 == 0) {
                gfx_fill_rect(cvx, cvy, 2, BDIM_LINE_H, CLR_CURSOR);
            } else if (bd.mode == MODE_NORMAL) {
                gfx_fill_rect(cvx, cvy, BDIM_CHAR_W, 2, CLR_CURSOR);
            }
        }
    }

    int sb_y = y + text_h;
    gfx_fill_rect(x, sb_y, w, BDIM_STATUS_H, 0x252526);
    gfx_draw_hline(x, sb_y, w, 0x333333);

    const char* mode_str = "";
    uint32_t mode_color = 0;
    switch (bd.mode) {
        case MODE_NORMAL:  mode_str = "NORMAL";  mode_color = CLR_STATUS_N; break;
        case MODE_INSERT:  mode_str = "INSERT";  mode_color = CLR_STATUS_I; break;
        case MODE_VISUAL:  mode_str = "VISUAL";  mode_color = CLR_STATUS_V; break;
        case MODE_COMMAND: mode_str = "CMD";     mode_color = CLR_STATUS_C; break;
    }
    gfx_fill_rect(x + 4, sb_y + 2, 60, BDIM_STATUS_H - 4, mode_color);
    gfx_draw_string_transparent(x + 8, sb_y + 5, mode_str, 0xFFFFFF);

    char info[64];
    int ii = 0;
    const char* fn = bd.filename[0] ? bd.filename : "[No Name]";
    while (fn[ii] && ii < 30) { info[ii] = fn[ii]; ii++; }
    info[ii] = 0;
    gfx_draw_string_transparent(x + 72, sb_y + 5, info, 0xCCCCCC);

    if (bd.dirty) gfx_draw_string_transparent(x + 72 + ii * 8 + 8, sb_y + 5, "[+]", CLR_DIRTY);

    char pos[32];
    ii = 0;
    pos[ii++] = 'L'; pos[ii++] = 'n'; pos[ii++] = ' ';
    int num = bd.cy + 1;
    char tmp[12]; int ti = 0;
    if (num == 0) tmp[ti++] = '0';
    else { int nt = num; while (nt > 0) { ti++; nt /= 10; } nt = num; tmp[ti] = 0; for (int j = ti-1; j>=0; j--) { tmp[j] = '0'+(nt%10); nt/=10; } }
    for (int j = 0; j < ti; j++) pos[ii++] = tmp[j];
    pos[ii++] = ','; pos[ii++] = ' ';
    pos[ii++] = 'C'; pos[ii++] = 'o'; pos[ii++] = 'l'; pos[ii++] = ' ';
    num = bd.cx + 1;
    ti = 0;
    if (num == 0) tmp[ti++] = '0';
    else { int nt = num; while (nt > 0) { ti++; nt /= 10; } nt = num; tmp[ti] = 0; for (int j = ti-1; j>=0; j--) { tmp[j] = '0'+(nt%10); nt/=10; } }
    for (int j = 0; j < ti; j++) pos[ii++] = tmp[j];
    pos[ii] = 0;
    gfx_draw_string_transparent(x + w - 140, sb_y + 5, pos, 0x858585);

    if (bd.cmd_active) {
        int cby = y + text_h - BDIM_CMD_H;
        gfx_fill_rect(x, cby, w, BDIM_CMD_H, CLR_CMDBG);
        gfx_draw_hline(x, cby, w, 0x333333);
        gfx_draw_string_transparent(x + 6, cby + 5, ":", 0xCCCCCC);
        gfx_draw_string_transparent(x + 14, cby + 5, bd.cmd_buf, CLR_CMDTXT);
    }

    if (bd.cmd_msg_timer > 0) {
        gfx_draw_string_transparent(x + 6, sb_y - BDIM_LINE_H - 4, bd.cmd_msg, 0xCCCCCC);
    }

    if (bd.show_help) {
        int hx = x + 20, hy = y + 10, hw = w - 40, hh = h - 60;
        gfx_draw_shadow(hx, hy, hw, hh, 15);
        gfx_fill_rect(hx, hy, hw, hh, 0x2D2D30);
        gfx_draw_rect_outline(hx, hy, hw, hh, 1, 0x555555);
        const char* help_lines[] = {
            " BDIM HELP",
            "",
            " i        Insert mode",
            " Esc      Return to Normal mode",
            " :        Command mode",
            " h/j/k/l  Move cursor",
            " w/b      Word forward/back",
            " 0/$      Start/End of line",
            " gg/G     Start/End of file",
            " dd       Delete line",
            " yy       Yank (copy) line",
            " p        Paste below",
            " u        Undo (simple)",
            " x        Delete char",
            " v        Visual mode",
            " /        Search",
            " n/N      Next/Prev search match",
            "",
            " :w       Save file",
            " :q       Quit",
            " :wq      Save and quit",
            " :q!      Force quit",
            " :help    Show this help",
            0
        };
        int li = 0;
        for (int hi = 0; help_lines[hi]; hi++) {
            if (hy + 20 + li * BDIM_LINE_H < hy + hh - 10) {
                uint32_t hc = (help_lines[hi][0] == ' ' || help_lines[hi][0] == 0) ? 0x888888 : 0xFFFFFF;
                gfx_draw_string_transparent(hx + 10, hy + 10 + li * BDIM_LINE_H, help_lines[hi], hc);
                li++;
            }
        }
    }
}

static void bd_cmd_execute(void) {
    if (bd.cmd_len == 0) { bd.cmd_active = 0; return; }
    bd.cmd_buf[bd.cmd_len] = 0;
    if (strcmp(bd.cmd_buf, "w") == 0) {
        bd_save();
    } else if (strcmp(bd.cmd_buf, "q") == 0) {
        if (bd.dirty) { bd_set_msg("Unsaved changes! Use :q! to force"); }
        else { wm_close_window(bd.win_id); bd.open = 0; }
    } else if (strcmp(bd.cmd_buf, "q!") == 0) {
        wm_close_window(bd.win_id); bd.open = 0;
    } else if (strcmp(bd.cmd_buf, "wq") == 0) {
        bd_save();
        if (!bd.dirty) { wm_close_window(bd.win_id); bd.open = 0; }
    } else if (strcmp(bd.cmd_buf, "help") == 0) {
        bd.show_help = !bd.show_help;
    } else {
        bd_set_msg("Unknown command");
    }
    bd.cmd_active = 0;
    bd.cmd_len = 0;
    bd.mode = MODE_NORMAL;
}

static void bd_handle_normal_key(unsigned char k) {
    static int prefix = 0;
    if (is_digit(k) && k != '0') {
        prefix = prefix * 10 + (k - '0');
        return;
    }
    int rep = prefix > 0 ? prefix : 1;
    prefix = 0;

    switch (k) {
        case 'i': bd.mode = MODE_INSERT; break;
        case 'a':
            if (bd.cx < bd.line_len[bd.cy] || bd.line_len[bd.cy] == 0) bd_cursor_right();
            bd.mode = MODE_INSERT; break;
        case 'o':
            bd_ensure_line(bd.line_count);
            bd_insert_line(bd.cy + 1);
            bd.cy++; bd.cx = 0;
            bd.dirty = 1;
            bd.mode = MODE_INSERT; break;
        case 'O':
            bd_ensure_line(bd.line_count);
            bd_insert_line(bd.cy);
            bd.cx = 0;
            bd.dirty = 1;
            bd.mode = MODE_INSERT; break;
        case 'h': for (int i = 0; i < rep; i++) bd_cursor_left(); break;
        case 'j': for (int i = 0; i < rep; i++) bd_cursor_down(); break;
        case 'k': for (int i = 0; i < rep; i++) bd_cursor_up(); break;
        case 'l': for (int i = 0; i < rep; i++) bd_cursor_right(); break;
        case 'w': bd_cursor_word_start(); break;
        case 'b': bd_cursor_word_end(); break; // simplified
        case '0': bd_cursor_line_start(); break;
        case '$': bd_cursor_line_end(); break;
        case 'x': for (int i = 0; i < rep; i++) { bd_delete_char_at(bd.cy, bd.cx); bd.dirty = 1; } break;
        case 'X': for (int i = 0; i < rep; i++) { if (bd.cx > 0) { bd_delete_char_at(bd.cy, --bd.cx); bd.dirty = 1; } } break;
        case 'd': {
            char next = get_key();
            if (next == 'd') { bd_delete_line_op(); }
            break;
        }
        case 'y': {
            char next = get_key();
            if (next == 'y') { bd_yank_line(); }
            break;
        }
        case 'p': bd_paste_line(); break;
        case 'P': {
            if (bd.clip_len > 0) {
                bd_insert_line(bd.cy);
                for (int i = 0; i < bd.clip_len && i < BDIM_MAX_COL - 1; i++)
                    bd.lines[bd.cy][i] = bd.clipboard[i];
                bd.line_len[bd.cy] = bd.clip_len;
                bd.lines[bd.cy][bd.line_len[bd.cy]] = 0;
                bd.dirty = 1;
            }
            break;
        }
        case 'u': bd_set_msg("Undo not implemented"); break;
        case 'v': bd.mode = MODE_VISUAL; bd.vis_start_line = bd.cy; bd.vis_start_col = bd.cx; break;
        case 'V': bd.mode = MODE_VISUAL; bd.vis_start_line = bd.cy; bd.vis_start_col = 0; break;
        case 'D': {
            bd.line_len[bd.cy] = bd.cx;
            bd.lines[bd.cy][bd.cx] = 0;
            bd.dirty = 1;
            break;
        }
        case 'C': {
            bd.line_len[bd.cy] = bd.cx;
            bd.lines[bd.cy][bd.cx] = 0;
            bd.dirty = 1;
            bd.mode = MODE_INSERT;
            break;
        }
        case 'r': {
            char next = get_key();
            if (next > 0 && bd.cx < bd.line_len[bd.cy]) {
                bd.lines[bd.cy][bd.cx] = next;
                bd.dirty = 1;
                bd_cursor_right();
            }
            break;
        }
        case 'J': {
            bd_join_lines(bd.cy);
            bd.dirty = 1;
            break;
        }
        case 'g': {
            char next = get_key();
            if (next == 'g') { bd_cursor_file_start(); }
            break;
        }
        case 'G': bd_cursor_file_end(); break;
        case '/':
            bd.mode = MODE_COMMAND;
            bd.cmd_active = 1;
            bd.cmd_len = 0;
            bd.cmd_buf[0] = '/';
            bd.cmd_len = 1;
            bd.search_active = 0;
            break;
        case 'n':
            if (bd.search_active && bd.search_len > 0) {
                int found = 0;
                for (int l = bd.cy; l < bd.line_count && !found; l++) {
                    int start = (l == bd.cy) ? bd.cx + 1 : 0;
                    for (int c = start; c <= bd.line_len[l] - bd.search_len; c++) {
                        int match = 1;
                        for (int si = 0; si < bd.search_len; si++)
                            if (bd.lines[l][c + si] != bd.search[si]) { match = 0; break; }
                        if (match) { bd.cy = l; bd.cx = c; bd.search_pos = c; found = 1; break; }
                    }
                }
                if (!found) {
                    for (int l = 0; l <= bd.cy && !found; l++) {
                        int end = (l == bd.cy) ? bd.cx : bd.line_len[l] - bd.search_len;
                        for (int c = 0; c <= end; c++) {
                            int match = 1;
                            for (int si = 0; si < bd.search_len; si++)
                                if (bd.lines[l][c + si] != bd.search[si]) { match = 0; break; }
                            if (match) { bd.cy = l; bd.cx = c; bd.search_pos = c; found = 1; break; }
                        }
                    }
                }
                if (!found) bd_set_msg("Pattern not found");
            }
            break;
        case 'N':
            if (bd.search_active && bd.search_len > 0) {
                int found = 0;
                for (int l = bd.cy; l >= 0 && !found; l--) {
                    int end = (l == bd.cy) ? bd.cx - bd.search_len : bd.line_len[l] - bd.search_len;
                    for (int c = end; c >= 0; c--) {
                        int match = 1;
                        for (int si = 0; si < bd.search_len; si++)
                            if (bd.lines[l][c + si] != bd.search[si]) { match = 0; break; }
                        if (match) { bd.cy = l; bd.cx = c; bd.search_pos = c; found = 1; break; }
                    }
                }
                if (!found) { bd_cursor_file_end(); bd_set_msg("Pattern not found"); }
            }
            break;
        case ':':
            bd.mode = MODE_COMMAND;
            bd.cmd_active = 1;
            bd.cmd_len = 0;
            bd.cmd_buf[0] = 0;
            break;
        case 27: break;
        case 128: bd_cursor_up(); break;
        case 129: bd_cursor_down(); break;
        case 130: bd_cursor_left(); break;
        case 131: bd_cursor_right(); break;
        case 133: bd_page_up(); break;
        case 134: bd_page_down(); break;
        case 132: if (bd.show_help) bd.show_help = 0; break;
        default: break;
    }
}

static void bd_handle_insert_key(unsigned char k) {
    if (k == 27) { bd.mode = MODE_NORMAL; return; }
    if (k == '\b') {
        if (bd.cx > 0) {
            bd_delete_char_at(bd.cy, --bd.cx);
            bd.dirty = 1;
        } else if (bd.cy > 0) {
            bd_join_lines(bd.cy - 1);
            bd.dirty = 1;
        }
    } else if (k == '\n') {
        bd_split_line(bd.cy, bd.cx);
        bd.cy++; bd.cx = 0;
        bd.dirty = 1;
    } else if (k == '\t') {
        bd_indent_line();
        bd.dirty = 1;
    } else if (k >= 32 && k <= 126) {
        bd_insert_char_at(bd.cy, bd.cx, k);
        if (bd.cx < BDIM_MAX_COL - 1) bd.cx++;
        bd.dirty = 1;
    } else if (k == 128) { bd_cursor_up(); }
    else if (k == 129) { bd_cursor_down(); }
    else if (k == 130) { bd_cursor_left(); }
    else if (k == 131) { bd_cursor_right(); }
    else if (k == 133) { bd_page_up(); }
    else if (k == 134) { bd_page_down(); }
}

static void bd_handle_visual_key(unsigned char k) {
    if (k == 27) { bd.mode = MODE_NORMAL; return; }
    if (k == 'h') bd_cursor_left();
    else if (k == 'j') bd_cursor_down();
    else if (k == 'k') bd_cursor_up();
    else if (k == 'l') bd_cursor_right();
    else if (k == 'd') { bd_clip_cut(); bd.dirty = 1; }
    else if (k == 'x') { bd_clip_cut(); bd.dirty = 1; }
    else if (k == 'y') { bd_clip_copy(); bd.mode = MODE_NORMAL; }
    else if (k == ':') { bd.mode = MODE_COMMAND; bd.cmd_active = 1; bd.cmd_len = 0; bd.cmd_buf[0] = 0; }
    else if (k == 128) bd_cursor_up();
    else if (k == 129) bd_cursor_down();
    else if (k == 130) bd_cursor_left();
    else if (k == 131) bd_cursor_right();
    else if (k == 133) bd_page_up();
    else if (k == 134) bd_page_down();
}

static void bd_on_key(int id, char key_in) {
    (void)id;
    unsigned char k = (unsigned char)key_in;
    if (!bd.open) return;

    if (bd.cmd_msg_timer > 0 && k > 0) bd.cmd_msg_timer = 0;

    if (bd.show_help) {
        if (k == 27 || k == 132) bd.show_help = 0;
        return;
    }

    if (bd.cmd_active) {
        if (k == 27) { bd.cmd_active = 0; bd.cmd_len = 0; bd.mode = MODE_NORMAL; return; }
        if (k == '\n') { bd_cmd_execute(); return; }
        if (k == '\b') { if (bd.cmd_len > 0) bd.cmd_buf[--bd.cmd_len] = 0; return; }
        if (k >= 32 && k <= 126 && bd.cmd_len < 126) { bd.cmd_buf[bd.cmd_len++] = k; bd.cmd_buf[bd.cmd_len] = 0; }
        if (bd.cmd_buf[0] == '/' && bd.cmd_len > 1) {
            for (int i = 1; i < bd.cmd_len; i++) bd.search[i-1] = bd.cmd_buf[i];
            bd.search_len = bd.cmd_len - 1;
            bd.search[bd.search_len] = 0;
            bd.search_active = 1;
        }
        return;
    }

    switch (bd.mode) {
        case MODE_NORMAL: bd_handle_normal_key(k); break;
        case MODE_INSERT: bd_handle_insert_key(k); break;
        case MODE_VISUAL: bd_handle_visual_key(k); break;
    }
}

static void bd_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)x; (void)y; (void)w; (void)h; (void)vx; (void)vy;
    bd_render();
}

static void bd_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

static void bd_on_mouse(int id, int mx, int my, int mb) {
    (void)id;
    if (!bd.open) return;
    wm_window_t* win = wm_get_window(bd.win_id);
    if (!win) return;

    int text_h = win->h - WM_TITLEBAR_H - BDIM_STATUS_H;
    int vis_lines = text_h / BDIM_LINE_H;
    if (vis_lines < 1) vis_lines = 1;

    int gutter = BDIM_GUTTER_W;
    int col = (mx - gutter - 4) / BDIM_CHAR_W + bd.scroll_x;
    int row = my / BDIM_LINE_H + bd.scroll_y;

    if (my >= text_h) return;

    if (col < 0) col = 0;
    if (row >= bd.line_count) row = bd.line_count - 1;
    if (row < 0) row = 0;
    if (col > bd.line_len[row]) col = bd.line_len[row];

    if (mb & 1) {
        if (bd.mode == MODE_NORMAL || bd.mode == MODE_INSERT) {
            if (!bd.mouse_selecting) {
                bd.mouse_selecting = 1;
                bd.mouse_down_x = mx; bd.mouse_down_y = my;
            }
            bd.cy = row; bd.cx = col;
        } else if (bd.mode == MODE_VISUAL) {
            bd.cy = row; bd.cx = col;
        }
    } else {
        if (bd.mouse_selecting && (mx != bd.mouse_down_x || my != bd.mouse_down_y)) {
            if (bd.mode == MODE_NORMAL) {
                bd.mode = MODE_VISUAL;
                bd.vis_start_line = bd.cy;
                bd.vis_start_col = bd.cx;
            }
        }
        bd.mouse_selecting = 0;
    }
    if ((mb & 1) && bd.mode == MODE_NORMAL) {
        bd.cy = row; bd.cx = col;
    }
}

void bdim_new(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    bd.mode = MODE_NORMAL;
    bd.open = 0;
    bd.line_count = 0;
    bd.cx = 0; bd.cy = 0;
    bd.scroll_x = 0; bd.scroll_y = 0;
    bd.dirty = 0;
    bd.clip_len = 0;
    bd.search_active = 0;
    bd.search_len = 0;
    bd.cmd_active = 0;
    bd.cmd_len = 0;
    bd.cmd_msg[0] = 0;
    bd.cmd_msg_timer = 0;
    bd.mouse_selecting = 0;
    bd.show_help = 0;
    bd.quit_on_clean = 0;
    bd.in_block_comment = 0;

    bd.filename[0] = 0;
    bd_ensure_line(0);

    int win_w = 720, win_h = 480;
    bd.win_id = wm_open_window(
        (fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
        "BDIM - [No Name]", 0x3281C6,
        bd_on_render, bd_on_key, bd_on_resize
    );
    if (bd.win_id >= 0) {
        wm_set_mouse_handler(bd.win_id, bd_on_mouse);
        bd.open = 1;
    }
}

void bdim_open(const char* name) {
    bdim_new();
    if (bd.open && bd.win_id >= 0) {
        bd_load(name);
        wm_window_t* win = wm_get_window(bd.win_id);
        if (win) {
            char title[72] = "BDIM - ";
            int ti = 7;
            int si = 0;
            while (bd.filename[si] && ti < 70) { title[ti++] = bd.filename[si]; si++; }
            title[ti] = 0;
            int j = 0;
            while (title[j] && j < 63) { win->title[j] = title[j]; j++; }
            win->title[j] = 0;
        }
    }
}

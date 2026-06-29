// ============================================================
//  BEDI OS — Terminal Emulator (multi-window)
// ============================================================
#include "drivers/video/gfx.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "commands/commands.h"
#include "kernel/time/timer.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int gui_terminal_active = 0;

#define TERM_ROWS 512
#define TERM_COLS 80
#define TERM_MAX_WINDOWS 8
#define HIST_SIZE 32

// Per-window terminal state
typedef struct {
    int win_id;
    char buf[TERM_ROWS][TERM_COLS];
    uint32_t colors[TERM_ROWS][TERM_COLS];
    int cursor_x;
    int cursor_y;
    int scroll_offset;
    int editor_cursor_x;
    int editor_cursor_y;
    char input_buf[256];
    int input_len;
    int input_active;
    char* input_target_buf;
    int input_max_len;
    char history[HIST_SIZE][256];
    int hist_count;
    int hist_idx;
    uint32_t current_color;
} term_instance_t;

static term_instance_t g_terms[TERM_MAX_WINDOWS];
static int g_term_count = 0;
static term_instance_t* g_active_term = 0;

void term_set_active_instance(term_instance_t* t) { g_active_term = t; }
term_instance_t* term_get_active_instance(void)    { return g_active_term; }

static int term_get_visible_rows(int h) {
    int rows = (h - 8) / 16;
    return (rows > 0) ? rows : 1;
}

static term_instance_t* term_find_by_win(int win_id) {
    for (int i = 0; i < g_term_count; i++) {
        if (g_terms[i].win_id == win_id) return &g_terms[i];
    }
    return NULL;
}

static term_instance_t* term_find_by_focus(void) {
    int fid = wm_get_focused();
    if (fid < 0 && g_term_count > 0) return &g_terms[g_term_count - 1];
    return term_find_by_win(fid);
}

static void term_instance_init(term_instance_t* t, int win_id) {
    memset(t, 0, sizeof(*t));
    t->win_id = win_id;
    t->current_color = 0xC9D1D9;
    for (int y = 0; y < TERM_ROWS; y++)
        for (int x = 0; x < TERM_COLS; x++)
            t->colors[y][x] = 0xC9D1D9;
}

static void term_instance_clear(term_instance_t* t) {
    for (int y = 0; y < TERM_ROWS; y++)
        for (int x = 0; x < TERM_COLS; x++) {
            t->buf[y][x] = 0;
            t->colors[y][x] = 0xC9D1D9;
        }
    t->cursor_x = 0;
    t->cursor_y = 0;
    t->scroll_offset = 0;
}

void gui_terminal_set_color(uint32_t c) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    t->current_color = c;
}

void gui_terminal_clear(void) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    term_instance_clear(t);
}

static void term_scroll(term_instance_t* t) {
    if (t->cursor_y < TERM_ROWS - 1) {
        t->cursor_y++;
    } else {
        for (int y = 0; y < TERM_ROWS - 1; y++) {
            for (int x = 0; x < TERM_COLS; x++) {
                t->buf[y][x] = t->buf[y + 1][x];
                t->colors[y][x] = t->colors[y + 1][x];
            }
        }
        for (int x = 0; x < TERM_COLS; x++) {
            t->buf[TERM_ROWS - 1][x] = 0;
            t->colors[TERM_ROWS - 1][x] = 0xC9D1D9;
        }
    }
    t->cursor_x = 0;
}

static void term_putc(term_instance_t* t, char c) {
    if (c == '\n') {
        term_scroll(t);
    } else if (c == '\r') {
        t->cursor_x = 0;
    } else if (c == '\b') {
        if (t->cursor_x > 0) {
            t->cursor_x--;
            t->buf[t->cursor_y][t->cursor_x] = 0;
        }
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) term_putc(t, ' ');
    } else if ((uint8_t)c >= 32) {
        if (t->cursor_x >= TERM_COLS) term_scroll(t);
        t->buf[t->cursor_y][t->cursor_x] = c;
        t->colors[t->cursor_y][t->cursor_x++] = t->current_color;
    }
}

void gui_terminal_print(const char* message) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    while (*message) {
        if (message[0] == '\e' && message[1] == '[') {
            message += 2;
            int code = 0;
            while (*message >= '0' && *message <= '9') {
                code = code * 10 + (*message - '0');
                message++;
            }
            if (*message == 'm') {
                message++;
                if (code == 0) t->current_color = 0xC9D1D9;
                else if (code == 31 || code == 91) t->current_color = 0xF85149;
                else if (code == 32 || code == 92) t->current_color = 0x3FB950;
                else if (code == 33 || code == 93) t->current_color = 0xE3B341;
                else if (code == 34 || code == 94) t->current_color = 0x58A6FF;
                else if (code == 35 || code == 95) t->current_color = 0xBC8CFF;
                else if (code == 36 || code == 96) t->current_color = 0x39D2C0;
                else if (code == 37 || code == 97) t->current_color = 0xF0F6FC;
                else if (code == 90) t->current_color = 0x6E7681;
                continue;
            }
        }
        term_putc(t, *message++);
    }
}

void gui_terminal_backspace(void) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    term_putc(t, '\b');
}

void gui_terminal_start_input(char* buf, int max_len) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    t->input_active = 1;
    t->input_target_buf = buf;
    t->input_max_len = max_len;
    t->input_len = 0;
    t->input_buf[0] = 0;
}

int gui_terminal_is_input_active(void) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return 0;
    return t->input_active;
}

int gui_terminal_input(char* buf, int max_len) {
    gui_terminal_start_input(buf, max_len);
    while (1) {
        term_instance_t* t = term_find_by_focus();
        if (!t || !t->input_active) break;
        extern int wm_tick(void);
        wm_tick();
        sleep_ms(1);
    }
    return 0;
}

// ── bdim / editor accessors ─────────────────────────────────
void term_set_cursor(int x, int y) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    t->cursor_x = x; t->cursor_y = y;
}
void term_set_scroll(int scroll) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    t->scroll_offset = scroll;
}
void term_clear(void) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    term_instance_clear(t);
}
void term_putc_at(int x, int y, char c, uint32_t color) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    if (x >= 0 && x < TERM_COLS && y >= 0 && y < TERM_ROWS) {
        t->buf[y][x] = c;
        t->colors[y][x] = color;
    }
}
char term_getc_at(int x, int y) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return 0;
    if (x >= 0 && x < TERM_COLS && y >= 0 && y < TERM_ROWS) return t->buf[y][x];
    return 0;
}
int term_get_width(void) { return TERM_COLS; }
int term_get_height(void) { return TERM_ROWS; }
int term_get_cols(void) { return TERM_COLS; }
int term_get_rows(void) { return TERM_ROWS; }
static char term_status_buf[64];
void term_set_status(const char* s) {
    int i = 0;
    while (s[i] && i < 63) { term_status_buf[i] = s[i]; i++; }
    term_status_buf[i] = 0;
}
const char* term_get_status(void) { return term_status_buf; }
void term_exit_editor(void) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    term_status_buf[0] = 0;
    print_prompt();
}
void term_set_current_color(uint32_t c) {
    term_instance_t* t = term_find_by_focus();
    if (!t) return;
    t->current_color = c;
}
void term_set_color_at(int x, int y, uint32_t color) {
    term_instance_t* t = g_active_term ? g_active_term : term_find_by_focus();
    if (!t) return;
    if (x >= 0 && x < TERM_COLS && y >= 0 && y < TERM_ROWS) {
        t->colors[y][x] = color;
    }
}
void term_set_char_at(int x, int y, char c) {
    term_instance_t* t = g_active_term ? g_active_term : term_find_by_focus();
    if (!t) return;
    if (x >= 0 && x < TERM_COLS && y >= 0 && y < TERM_ROWS) {
        t->buf[y][x] = c;
    }
}
void term_set_editor_cursor(int x, int y) {
    term_instance_t* t = g_active_term ? g_active_term : term_find_by_focus();
    if (!t) return;
    t->editor_cursor_x = x; t->editor_cursor_y = y;
}

extern void print_prompt(void);
extern void handle_tab(char* buffer, int* index);

static void term_on_key(int id, char key_in) {
    term_instance_t* t = term_find_by_win(id);
    if (!t) return;

    // Input mode
    if (t->input_active) {
        unsigned char k = (unsigned char)key_in;
        if (k == '\n') {
            gui_terminal_print("\n");
            t->input_target_buf[t->input_len] = 0;
            t->input_active = 0;
        } else if (k == '\b') {
            if (t->input_len > 0) {
                t->input_len--;
                t->input_target_buf[t->input_len] = 0;
                if (t->cursor_x > 0) {
                    t->cursor_x--;
                    t->buf[t->cursor_y][t->cursor_x] = 0;
                }
            }
        } else if (k >= 32 && k <= 126 && t->input_len < t->input_max_len - 1) {
            t->input_target_buf[t->input_len++] = k;
            t->input_target_buf[t->input_len] = 0;
            char s[2] = {(char)k, 0};
            term_putc(t, k);
        }
        return;
    }

    unsigned char k = (unsigned char)key_in;
    wm_window_t* win = wm_get_window(id);
    int vis_rows = term_get_visible_rows(win ? win->h - WM_TITLEBAR_H : 480);

    extern int keyboard_is_key_down(uint8_t scancode);
    int is_shift = keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36);

    if (k == '\b') {
        if (t->input_len > 0) {
            t->input_len--;
            t->input_buf[t->input_len] = 0;
            if (t->cursor_x > 0) {
                t->cursor_x--;
                t->buf[t->cursor_y][t->cursor_x] = 0;
            }
        }
    } else if (k == '\t') {
        handle_tab(t->input_buf, &t->input_len);
    } else if (k == '\n') {
        t->input_buf[t->input_len] = 0;
        term_putc(t, '\n');
        if (t->input_len > 0) {
            if (t->hist_count == 0 || strcmp(t->input_buf, t->history[0]) != 0) {
                for (int i = HIST_SIZE - 1; i > 0; i--)
                    memcpy(t->history[i], t->history[i - 1], 256);
                memcpy(t->history[0], t->input_buf, 256);
                if (t->hist_count < HIST_SIZE) t->hist_count++;
            }
            t->hist_idx = -1;
            if (strcmp(t->input_buf, "clear") == 0) {
                term_instance_clear(t);
            } else {
                execute_command(t->input_buf);
            }
        }
        t->input_len = 0;
        t->input_buf[0] = 0;
        // print prompt into this instance
        // Reuse global print_prompt which calls gui_terminal_print, now routed to focused window
        // Force focus to this instance before printing prompt
        print_prompt();
    } else if (k == 128) { // KEY_UP
        if (is_shift) {
            if (t->scroll_offset < t->cursor_y) t->scroll_offset += 2;
        } else if (t->hist_count > 0) {
            for (int i = 0; i < t->input_len; i++) term_putc(t, '\b');
            t->input_len = 0;
            if (t->hist_idx < t->hist_count - 1) t->hist_idx++;
            while (t->history[t->hist_idx][t->input_len]) {
                t->input_buf[t->input_len] = t->history[t->hist_idx][t->input_len];
                term_putc(t, t->input_buf[t->input_len]);
                t->input_len++;
            }
            t->input_buf[t->input_len] = 0;
        }
    } else if (k == 129) { // KEY_DOWN
        if (is_shift) {
            if (t->scroll_offset > 0) t->scroll_offset -= 2;
            if (t->scroll_offset < 0) t->scroll_offset = 0;
        } else if (t->hist_idx >= 0) {
            for (int i = 0; i < t->input_len; i++) term_putc(t, '\b');
            t->input_len = 0;
            t->hist_idx--;
            if (t->hist_idx >= 0) {
                while (t->history[t->hist_idx][t->input_len]) {
                    t->input_buf[t->input_len] = t->history[t->hist_idx][t->input_len];
                    term_putc(t, t->input_buf[t->input_len]);
                    t->input_len++;
                }
            }
            t->input_buf[t->input_len] = 0;
        }
    } else if (k == 133) { // KEY_PAGE_UP
        if (is_shift) {
            int max_scroll = t->cursor_y - vis_rows + 1;
            if (max_scroll < 0) max_scroll = 0;
            t->scroll_offset += vis_rows;
            if (t->scroll_offset > max_scroll) t->scroll_offset = max_scroll;
        }
    } else if (k == 134) { // KEY_PAGE_DOWN
        if (is_shift) {
            t->scroll_offset -= vis_rows;
            if (t->scroll_offset < 0) t->scroll_offset = 0;
        }
    } else if (k >= 32 && k <= 126) {
        if (t->input_len < 255) {
            t->input_buf[t->input_len++] = k;
            term_putc(t, k);
        }
    }
}

static void term_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    term_instance_t* t = term_find_by_win(id);
    if (!t) return;

    gfx_fill_rect(x, y, w, h, 0x0D1117);

    int vis_rows = term_get_visible_rows(h);
    int content_rows = vis_rows;
    uint32_t sb_bg = 0x0D1117;
    uint32_t sb_fg = 0xC9D1D9;

    int base_y;
    base_y = t->cursor_y - content_rows + 1;
    if (base_y < 0) base_y = 0;
    base_y -= t->scroll_offset;
    if (base_y < 0) base_y = 0;

    int text_area_w = w - 12;
    int max_cols = text_area_w / 8;
    if (max_cols > TERM_COLS) max_cols = TERM_COLS;

    for (int r = 0; r < content_rows; r++) {
        int buf_r = base_y + r;
        if (buf_r >= TERM_ROWS || buf_r < 0) break;
        for (int c = 0; c < max_cols; c++) {
            if (t->buf[buf_r][c]) {
                char s[2] = {t->buf[buf_r][c], 0};
                gfx_draw_string_transparent(x + 6 + (c * 8), y + 4 + (r * 16), s, t->colors[buf_r][c]);
            }
        }
    }

    extern uint32_t timer_get_ms(void);
    if ((timer_get_ms() / 500) % 2 == 0) {
        if (t->scroll_offset == 0 && t->cursor_x < max_cols &&
            t->cursor_y >= base_y && t->cursor_y < base_y + content_rows) {
            int cx = x + 6 + (t->cursor_x * 8);
            int cy = y + 4 + ((t->cursor_y - base_y) * 16);
            gfx_fill_rect(cx, cy, 2, 16, 0x58A6FF);
        }
    }
}

static void term_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
    wm_clear_buttons(win_id);
}

void term_notify_closed(int win_id) {
    for (int i = 0; i < g_term_count; i++) {
        if (g_terms[i].win_id == win_id) {
            for (int j = i; j < g_term_count - 1; j++) g_terms[j] = g_terms[j + 1];
            g_term_count--;
            if (g_term_count == 0) gui_terminal_active = 0;
            return;
        }
    }
}

void terminal_app(void) {
    // Reuse a free slot if available
    int slot = -1;
    for (int i = 0; i < g_term_count; i++) {
        extern wm_window_t* wm_get_window(int);
        if (!wm_get_window(g_terms[i].win_id)) {
            slot = i;
            break;
        }
    }
    if (slot == -1 && g_term_count >= TERM_MAX_WINDOWS) {
        if (g_term_count > 0) wm_bring_to_front(g_terms[g_term_count - 1].win_id);
        return;
    }

    int win_w = 660, win_h = 480;
    int win_id = wm_open_window(80 + ((slot >= 0 ? slot : g_term_count) * 20),
                                60 + ((slot >= 0 ? slot : g_term_count) * 20),
                                win_w, win_h, "Terminal", 0x58A6FF,
                                term_on_render, term_on_key, term_on_resize);
    if (win_id < 0) {
        if (g_term_count > 0) wm_bring_to_front(g_terms[g_term_count - 1].win_id);
        return;
    }

    term_instance_t* t;
    if (slot >= 0) {
        t = &g_terms[slot];
    } else {
        t = &g_terms[g_term_count++];
    }
    term_instance_init(t, win_id);
    term_instance_clear(t);

    gui_terminal_active = 1;
    t->current_color = 0xC9D1D9;
    memcpy(t->input_buf, "", 1);
    t->input_len = 0;

    gui_terminal_print("Welcome to BEDI OS.\n");
    gui_terminal_print("Type 'help' for available commands.\n\n");
    print_prompt();
}

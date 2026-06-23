// ============================================================
//  BEDI OS — Terminal Emulator
//  Professional scrollable terminal with command history,
//  ANSI color support, and clean rendering.
// ============================================================
#include "drivers/video/gfx.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "commands/commands.h"
#include "kernel/time/timer.h"
#include <stddef.h>

int gui_terminal_active = 0;

#define TERM_ROWS 512
#define TERM_COLS 80

static int term_get_visible_rows(int h) {
    int rows = (h - 8) / 16;
    return (rows > 0) ? rows : 1;
}

static char term_buf[TERM_ROWS][TERM_COLS];
static uint32_t term_colors[TERM_ROWS][TERM_COLS];
static int term_cursor_x = 0;
static int term_cursor_y = 0;
static int term_scroll_offset = 0;
static char input_buf[256];
static int input_len = 0;
static int term_win_id = -1;

static int input_active = 0;
static char* input_target_buf = NULL;
static int input_max_len = 0;

#define HIST_SIZE 32
static char history[HIST_SIZE][256];
static int hist_count = 0;
static int hist_idx = -1;

static uint32_t current_color = 0xC9D1D9;

void gui_terminal_set_color(uint32_t c) { current_color = c; }

void gui_terminal_clear(void) {
    for (int y = 0; y < TERM_ROWS; y++) {
        for (int x = 0; x < TERM_COLS; x++) {
            term_buf[y][x] = 0;
            term_colors[y][x] = 0xC9D1D9;
        }
    }
    term_cursor_x = 0;
    term_cursor_y = 0;
    term_scroll_offset = 0;
}

static void term_scroll(void) {
    if (term_cursor_y < TERM_ROWS - 1) {
        term_cursor_y++;
    } else {
        for (int y = 0; y < TERM_ROWS - 1; y++) {
            for (int x = 0; x < TERM_COLS; x++) {
                term_buf[y][x] = term_buf[y + 1][x];
                term_colors[y][x] = term_colors[y + 1][x];
            }
        }
        for (int x = 0; x < TERM_COLS; x++) {
            term_buf[TERM_ROWS - 1][x] = 0;
            term_colors[TERM_ROWS - 1][x] = 0xC9D1D9;
        }
    }
    term_cursor_x = 0;
}

static void term_putc(char c) {
    if (term_scroll_offset > 0) term_scroll_offset = 0;

    if (c == '\n') {
        term_scroll();
    } else if (c == '\r') {
        term_cursor_x = 0;
    } else if (c == '\b') {
        if (term_cursor_x > 0) term_cursor_x--;
        term_buf[term_cursor_y][term_cursor_x] = 0;
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) term_putc(' ');
    } else if ((uint8_t)c >= 32) {
        if (term_cursor_x >= TERM_COLS) term_scroll();
        term_buf[term_cursor_y][term_cursor_x] = c;
        term_colors[term_cursor_y][term_cursor_x++] = current_color;
    }
}

void gui_terminal_print(const char* message) {
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
                if (code == 0) current_color = 0xC9D1D9;
                else if (code == 31 || code == 91) current_color = 0xF85149;
                else if (code == 32 || code == 92) current_color = 0x3FB950;
                else if (code == 33 || code == 93) current_color = 0xE3B341;
                else if (code == 34 || code == 94) current_color = 0x58A6FF;
                else if (code == 35 || code == 95) current_color = 0xBC8CFF;
                else if (code == 36 || code == 96) current_color = 0x39D2C0;
                else if (code == 37 || code == 97) current_color = 0xF0F6FC;
                else if (code == 90) current_color = 0x6E7681;
                continue;
            }
        }
        term_putc(*message++);
    }
}

void gui_terminal_backspace(void) { term_putc('\b'); }

void gui_terminal_start_input(char* buf, int max_len) {
    input_active = 1;
    input_target_buf = buf;
    input_max_len = max_len;
    input_len = 0;
    input_buf[0] = 0;
}

int gui_terminal_is_input_active(void) {
    return input_active;
}

int gui_terminal_input(char* buf, int max_len) {
    gui_terminal_start_input(buf, max_len);
    while (input_active) {
        extern int wm_tick(void);
        wm_tick(); 
        sleep_ms(1);
    }
    return input_len;
}

extern void print_prompt(void);
extern void handle_tab(char* buffer, int* index);

static void term_on_key(int id, char key_in) {
    if (!gui_terminal_active) return;
    
    // Non-blocking input mode
    if (input_active) {
        unsigned char k = (unsigned char)key_in;
        if (k == '\n') {
            gui_terminal_print("\n");
            input_target_buf[input_len] = 0;
            input_active = 0; // Signal input completion
        } else if (k == '\b') {
            if (input_len > 0) {
                input_len--;
                input_target_buf[input_len] = 0;
                gui_terminal_backspace();
            }
        } else if (k >= 32 && k <= 126 && input_len < input_max_len - 1) {
            input_target_buf[input_len++] = k;
            input_target_buf[input_len] = 0;
            char s[2] = {(char)k, 0};
            gui_terminal_print(s);
        }
        return;
    }
    
    unsigned char k = (unsigned char)key_in;
    wm_window_t* win = wm_get_window(id);
    int vis_rows = term_get_visible_rows(win ? win->h - WM_TITLEBAR_H : 480);
    
    extern int keyboard_is_key_down(uint8_t scancode);
    int is_shift = keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36);

    if (k == '\b') {
        if (input_len > 0) {
            input_len--;
            input_buf[input_len] = 0;
            gui_terminal_backspace();
        }
    } else if (k == '\t') {
        handle_tab(input_buf, &input_len);
    } else if (k == '\n') {
        input_buf[input_len] = 0;
        gui_terminal_print("\n");
        if (input_len > 0) {
            if (hist_count == 0 || strcmp(input_buf, history[0]) != 0) {
                for (int i = HIST_SIZE - 1; i > 0; i--) {
                    for(int j=0; j<256; j++) history[i][j] = history[i-1][j];
                }
                for(int j=0; j<256; j++) history[0][j] = input_buf[j];
                if (hist_count < HIST_SIZE) hist_count++;
            }
            hist_idx = -1;
            
            if (strcmp(input_buf, "clear") == 0) {
                gui_terminal_clear();
            } else {
                execute_command(input_buf);
            }
        }
        input_len = 0;
        input_buf[0] = 0;
        print_prompt();
    } else if (k == 128) { // KEY_UP
        if (is_shift) {
            if (term_scroll_offset < term_cursor_y) term_scroll_offset += 2;
        } else if (hist_count > 0) {
            for(int i=0; i<input_len; i++) gui_terminal_backspace();
            input_len = 0;
            if (hist_idx < hist_count - 1) hist_idx++;
            while(history[hist_idx][input_len]) {
                input_buf[input_len] = history[hist_idx][input_len];
                char s[2] = {input_buf[input_len], 0};
                gui_terminal_print(s);
                input_len++;
            }
            input_buf[input_len] = 0;
        }
    } else if (k == 129) { // KEY_DOWN
        if (is_shift) {
            if (term_scroll_offset > 0) term_scroll_offset -= 2;
            if (term_scroll_offset < 0) term_scroll_offset = 0;
        } else if (hist_idx >= 0) {
            for(int i=0; i<input_len; i++) gui_terminal_backspace();
            input_len = 0;
            hist_idx--;
            if (hist_idx >= 0) {
                while(history[hist_idx][input_len]) {
                    input_buf[input_len] = history[hist_idx][input_len];
                    char s[2] = {history[hist_idx][input_len], 0};
                    gui_terminal_print(s);
                    input_len++;
                }
            }
            input_buf[input_len] = 0;
        }
    } else if (k == 133) { // KEY_PAGE_UP
        if (is_shift) {
            int max_scroll = term_cursor_y - vis_rows + 1;
            if (max_scroll < 0) max_scroll = 0;
            term_scroll_offset += vis_rows;
            if (term_scroll_offset > max_scroll) term_scroll_offset = max_scroll;
        }
    } else if (k == 134) { // KEY_PAGE_DOWN
        if (is_shift) {
            term_scroll_offset -= vis_rows;
            if (term_scroll_offset < 0) term_scroll_offset = 0;
        }
    } else if (k >= 32 && k <= 126) {
        if (input_len < 255) {
            input_buf[input_len++] = k;
            char s[2] = {(char)k, 0};
            gui_terminal_print(s);
        }
    }
}

static void term_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    gfx_fill_rect(x, y, w, h, 0x0D1117);
    
    int vis_rows = term_get_visible_rows(h);
    int base_y = term_cursor_y - vis_rows + 1;
    if (base_y < 0) base_y = 0;
    base_y -= term_scroll_offset;
    if (base_y < 0) base_y = 0;
    
    int text_area_w = w - 12;
    int max_cols = text_area_w / 8;
    if (max_cols > TERM_COLS) max_cols = TERM_COLS;
    
    for (int r = 0; r < vis_rows; r++) {
        int buf_r = base_y + r;
        if (buf_r >= TERM_ROWS || buf_r < 0) break;
        for (int c = 0; c < max_cols; c++) {
            if (term_buf[buf_r][c]) {
                char s[2] = {term_buf[buf_r][c], 0};
                gfx_draw_string_transparent(x + 6 + (c * 8), y + 4 + (r * 16), s, term_colors[buf_r][c]);
            }
        }
    }
    
    extern uint32_t timer_get_ms(void);
    if ((timer_get_ms() / 500) % 2 == 0) {
        if (term_scroll_offset == 0 && term_cursor_x < max_cols &&
            term_cursor_y >= base_y && term_cursor_y < base_y + vis_rows) {
            int cx = x + 6 + (term_cursor_x * 8);
            int cy = y + 4 + ((term_cursor_y - base_y) * 16);
            gfx_fill_rect(cx, cy, 2, 16, 0x58A6FF);
        }
    }
}

static void term_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
    wm_clear_buttons(win_id);
}

void terminal_app(void) {
    if (term_win_id >= 0) {
        wm_window_t* win = wm_get_window(term_win_id);
        if (win != 0 && (win->flags & 0x01)) {
            wm_bring_to_front(term_win_id);
            return;
        }
    }
    int win_w = 660, win_h = 480;
    term_win_id = wm_open_window(80, 60, win_w, win_h, "Terminal", 0x58A6FF, term_on_render, term_on_key, term_on_resize);
    
    term_on_resize(term_win_id, win_w, win_h);

    gui_terminal_active = 1;
    gui_terminal_clear();
    input_len = 0;
    input_buf[0] = 0;
    
    gui_terminal_print("Welcome to BEDI OS.\n");
    gui_terminal_print("Type 'help' for available commands.\n\n");
    print_prompt();
}

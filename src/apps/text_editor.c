// ============================================================
//  BEDI OS — Text Editor
//  Professional editor with line numbers, syntax-aware cursor,
//  Home/End navigation, Tab insertion, Ctrl+S save,
//  auto-scroll, and status bar with line/col/size info.
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/wm.h"
#include "drivers/input/keyboard.h"
#include "filesystem/filesystem.h"
#include "commands/commands.h"

static char ed_filename[64];
static char ed_content[16384];
static int ed_content_len = 0;
static int ed_cursor = 0;
static int ed_scroll_y = 0;
static int ed_win_id = -1;
static int ed_dirty = 0;
static int ed_selection_start = -1;
static int ed_naming = 0;
static char ed_name_buf[64];
static int ed_name_len = 0;

#define ED_LINE_H 16
#define ED_CHAR_W 8
#define ED_MAX_SIZE 16383
#define ED_GUTTER_W 44

static int ed_get_visible_lines(int h) {
    int bar_h = 24;
    int status_h = 22;
    int text_h = h - bar_h - status_h;
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

// Get the offset of the start of a given line
static int ed_line_start(int line) {
    int l = 0, pos = 0;
    while (pos < ed_content_len && l < line) {
        if (ed_content[pos] == '\n') l++;
        pos++;
    }
    return pos;
}

// Get the length of a line (not counting the newline)
static int ed_line_length(int line) {
    int start = ed_line_start(line);
    int len = 0;
    while (start + len < ed_content_len && ed_content[start + len] != '\n') len++;
    return len;
}

static void ed_format_number(int n, char* buf, int width) {
    // Right-align number in a field of given width
    char temp[8];
    int len = 0;
    if (n == 0) { temp[len++] = '0'; }
    else {
        int tmp = n;
        while (tmp > 0) { temp[len++] = (tmp % 10) + '0'; tmp /= 10; }
    }
    // Reverse
    for (int j = 0; j < len / 2; j++) { 
        char t = temp[j]; temp[j] = temp[len-1-j]; temp[len-1-j] = t; 
    }
    temp[len] = 0;
    
    // Pad with spaces
    int pad = width - len;
    int bi = 0;
    while (pad > 0) { buf[bi++] = ' '; pad--; }
    for (int j = 0; j < len; j++) buf[bi++] = temp[j];
    buf[bi] = 0;
}

static void ed_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    // Title/status bar at top
    int bar_h = 24;
    gfx_fill_rect(x, y, w, bar_h, 0x161B22);
    gfx_draw_hline(x, y + bar_h - 1, w, 0x30363D);
    
    // Filename
    gfx_draw_string_transparent(x + 10, y + 4, ed_filename, 0x58A6FF);
    if (ed_dirty) {
        int nl = strlen(ed_filename);
        gfx_draw_string_transparent(x + 10 + nl * 8 + 4, y + 4, "[modified]", 0xF85149);
    }
    
    // Keyboard shortcuts
    if (w > 200) {
        gfx_draw_string_transparent(x + w - 90, y + 4, "Ctrl+S Save", 0x484F58);
    }
    
    // Content area
    int text_y_start = y + bar_h;
    int status_h = 22;
    int text_h = h - bar_h - status_h;
    int vis_lines = ed_get_visible_lines(h);
    
    // Background for gutter
    gfx_fill_rect(x, text_y_start, ED_GUTTER_W, text_h, 0x0A0E14);
    // Gutter separator
    gfx_draw_vline(x + ED_GUTTER_W - 1, text_y_start, text_h, 0x21262D);
    
    // Content background
    gfx_fill_rect(x + ED_GUTTER_W, text_y_start, w - ED_GUTTER_W, text_h, 0x0D1117);
    
    int cursor_row, cursor_col;
    ed_get_cursor_pos(&cursor_row, &cursor_col);
    
    // Auto-scroll
    if (cursor_row < ed_scroll_y) ed_scroll_y = cursor_row;
    if (cursor_row >= ed_scroll_y + vis_lines) ed_scroll_y = cursor_row - vis_lines + 1;
    
    int vis_line = 0;
    int char_idx = 0;
    int line = 0;
    int col = 0;
    
    // Skip lines until scroll position
    for (int i = 0; i < ed_content_len && line < ed_scroll_y; i++) {
        if (ed_content[i] == '\n') line++;
        char_idx = i + 1;
    }
    
    vis_line = 0;
    line = ed_scroll_y;
    col = 0;
    
    int content_x = x + ED_GUTTER_W + 6;
    int max_chars_per_line = (w - ED_GUTTER_W - 14) / ED_CHAR_W;
    if (max_chars_per_line < 1) max_chars_per_line = 1;
    if (max_chars_per_line > 200) max_chars_per_line = 200;
    
    // Draw line number for first visible line
    char lnum[8];
    ed_format_number(line + 1, lnum, 4);
    uint32_t ln_color = (line == cursor_row) ? 0x8B949E : 0x484F58;
    gfx_draw_string_transparent(x + 4, text_y_start + vis_line * ED_LINE_H, lnum, ln_color);
    
    // Highlight current line
    if (cursor_row == line) {
        gfx_fill_rect(x + ED_GUTTER_W, text_y_start + vis_line * ED_LINE_H, w - ED_GUTTER_W, ED_LINE_H, 0x161B22);
    }
    
    for (int i = char_idx; i <= ed_content_len && vis_line < vis_lines; i++) {
        // Draw cursor
        if (i == ed_cursor) {
            extern uint32_t timer_get_ms(void);
            if ((timer_get_ms() / 500) % 2 == 0) {
                int cx = content_x + col * ED_CHAR_W;
                int cy = text_y_start + vis_line * ED_LINE_H;
                gfx_fill_rect(cx, cy, 2, ED_LINE_H, 0x58A6FF);
            }
        }
        
        if (i >= ed_content_len) break;
        
        if (ed_content[i] == '\n') {
            line++;
            col = 0;
            vis_line++;
            if (vis_line < vis_lines) {
                ed_format_number(line + 1, lnum, 4);
                ln_color = (line == cursor_row) ? 0x8B949E : 0x484F58;
                gfx_draw_string_transparent(x + 4, text_y_start + vis_line * ED_LINE_H, lnum, ln_color);
                
                // Highlight current line
                if (cursor_row == line) {
                    gfx_fill_rect(x + ED_GUTTER_W, text_y_start + vis_line * ED_LINE_H, w - ED_GUTTER_W, ED_LINE_H, 0x161B22);
                }
            }
        } else {
            if (col < max_chars_per_line) {
                char s[2] = {ed_content[i], 0};
                gfx_draw_string_transparent(content_x + col * ED_CHAR_W, text_y_start + vis_line * ED_LINE_H, s, 0xC9D1D9);
            }
            col++;
        }
    }
    
    // Bottom status bar
    int status_y = y + h - 22;
    gfx_fill_rect(x, status_y, w, 22, 0x161B22);
    gfx_draw_hline(x, status_y, w, 0x30363D);
    
    // Ln/Col info
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
    
    // Total lines
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
    
    // File size
    if (w > 450) {
        char temp[8]; itoa(ed_content_len, temp);
        char size_str[32]; int pi = 0;
        for (int j = 0; temp[j]; j++) size_str[pi++] = temp[j];
        size_str[pi++] = ' '; size_str[pi++] = 'B';
        size_str[pi] = 0;
        gfx_draw_string_transparent(x + w - 80, status_y + 3, size_str, 0x6E7681);
    }
    
    // Encoding indicator
    if (w > 550 && !ed_naming) {
        gfx_draw_string_transparent(x + w - 160, status_y + 3, "UTF-8", 0x484F58);
    }

    // Naming prompt
    if (ed_naming) {
        gfx_fill_rect(x + w - 260, status_y + 1, 254, 20, 0x0D1117);
        gfx_draw_rect_outline(x + w - 260, status_y + 1, 254, 20, 1, 0x58A6FF);
        char prompt[64] = "Name: ";
        int pi = 6;
        for (int j = 0; ed_name_buf[j] && pi < 62; j++) prompt[pi++] = ed_name_buf[j];
        prompt[pi] = 0;
        gfx_draw_string_transparent(x + w - 256, status_y + 4, prompt, 0xF0F6FC);
    }
}

static void ed_do_save(const char* name) {
    fs_delete(name);
    int fd = fs_create(name);
    if (fd >= 0) {
        fs_write(fd, ed_content, ed_content_len);
        fs_close(fd);
        ed_dirty = 0;
        // Update the stored filename
        int i = 0;
        while (name[i] && i < 63) { ed_filename[i] = name[i]; i++; }
        ed_filename[i] = 0;
    }
}

static void ed_save(void) {
    if (strcmp(ed_filename, "untitled.txt") == 0) {
        ed_naming = 1;
        ed_name_len = 0;
        ed_name_buf[0] = 0;
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

// Delete character at cursor (Del key behavior)
static void ed_delete_forward(void) {
    if (ed_cursor < ed_content_len) {
        for (int i = ed_cursor; i < ed_content_len - 1; i++)
            ed_content[i] = ed_content[i + 1];
        ed_content_len--;
        ed_dirty = 1;
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
            ed_do_save(ed_name_buf);
        } else if (k >= 32 && k <= 126 && ed_name_len < 62) {
            ed_name_buf[ed_name_len++] = k;
            ed_name_buf[ed_name_len] = 0;
        }
        return;
    }
    
    if (k == 19) { // Ctrl+S
        ed_save();
        return;
    }
    
    // Ctrl+A = select all / move to start
    if (k == 1) {
        ed_cursor = 0;
        ed_scroll_y = 0;
        return;
    }
    
    // Ctrl+E = move to end
    if (k == 5) {
        ed_cursor = ed_content_len;
        return;
    }
    
    if (k == '\b') {
        ed_delete_char();
    } else if (k == '\n') {
        ed_insert_char('\n');
    } else if (k == '\t') {
        // Insert 4 spaces (soft tab)
        for (int t = 0; t < 4; t++) ed_insert_char(' ');
    } else if (k == 128) { // KEY_UP
        int row, col;
        ed_get_cursor_pos(&row, &col);
        if (row > 0) {
            int prev_len = ed_line_length(row - 1);
            int target_col = col < prev_len ? col : prev_len;
            ed_cursor = ed_line_start(row - 1) + target_col;
        }
    } else if (k == 129) { // KEY_DOWN
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int total = ed_count_lines();
        if (row < total - 1) {
            int next_len = ed_line_length(row + 1);
            int target_col = col < next_len ? col : next_len;
            ed_cursor = ed_line_start(row + 1) + target_col;
        }
    } else if (k == 130) { // KEY_LEFT
        if (ed_cursor > 0) ed_cursor--;
    } else if (k == 131) { // KEY_RIGHT
        if (ed_cursor < ed_content_len) ed_cursor++;
    } else if (k == 133) { // KEY_PAGE_UP
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int new_row = row - vis_lines;
        if (new_row < 0) new_row = 0;
        int new_len = ed_line_length(new_row);
        int target_col = col < new_len ? col : new_len;
        ed_cursor = ed_line_start(new_row) + target_col;
    } else if (k == 134) { // KEY_PAGE_DOWN
        int row, col;
        ed_get_cursor_pos(&row, &col);
        int total = ed_count_lines();
        int new_row = row + vis_lines;
        if (new_row >= total) new_row = total - 1;
        int new_len = ed_line_length(new_row);
        int target_col = col < new_len ? col : new_len;
        ed_cursor = ed_line_start(new_row) + target_col;
    } else if (k >= 32 && k <= 126) {
        int row, col;
        ed_get_cursor_pos(&row, &col);
        
        int max_chars_per_line = (win_w - ED_GUTTER_W - 14) / ED_CHAR_W - 1;
        if (max_chars_per_line < 10) max_chars_per_line = 10;
        
        if (col >= max_chars_per_line) {
            ed_insert_char('\n');
        }
        ed_insert_char(k);
    }
}

static void ed_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

void text_editor_open(const char* filename) {
    // Copy filename
    int i = 0;
    while (filename[i] && i < 63) { ed_filename[i] = filename[i]; i++; }
    ed_filename[i] = 0;
    
    // Load file content
    ed_content_len = 0;
    ed_cursor = 0;
    ed_scroll_y = 0;
    ed_dirty = 0;
    ed_selection_start = -1;
    
    int fd = fs_open(filename, 0);
    if (fd >= 0) {
        ed_content_len = fs_read(fd, ed_content, ED_MAX_SIZE);
        if (ed_content_len < 0) ed_content_len = 0;
        fs_close(fd);
    }
    ed_content[ed_content_len] = 0;
    
    // Build window title
    char title[80] = "Edit: ";
    int ti = 6;
    for (int j = 0; ed_filename[j] && ti < 78; j++) title[ti++] = ed_filename[j];
    title[ti] = 0;
    
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 640, win_h = 480;
    ed_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                                title, 0x58A6FF, ed_on_render, ed_on_key, ed_on_resize);
}

// Create new empty file and open editor
void text_editor_new(const char* filename) {
    int fd = fs_create(filename);
    if (fd >= 0) fs_close(fd);
    text_editor_open(filename);
}
void text_editor(void) { text_editor_new("untitled.txt"); }

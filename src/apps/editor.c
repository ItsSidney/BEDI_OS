#include "../../include/editor.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/pcspeaker.h"
#include "filesystem.h"

#define CLR_ED_TEXT ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_ED_HDR  ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)
#define CLR_ED_NUM  ((VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_DARK_GREY)

static editor_t current_editor;
static int cursor_blink = 1;
static int blink_timer = 0;
static int save_indicator_timer = 0;

void init_editor(editor_t* ed) {
    for (int i = 0; i < EDITOR_MAX_LINES; i++) {
        for (int j = 0; j < EDITOR_LINE_LENGTH; j++) ed->lines[i][j] = 0;
    }
    ed->line_count = 1; ed->cursor_x = 0; ed->cursor_y = 0; ed->view_offset_y = 0; ed->view_offset_x = 0;
}

static void editor_save(editor_t* ed) {
    static char save_buf[MAX_FILE_SIZE];
    int pos = 0;
    for (int i = 0; i < ed->line_count; i++) {
        int j = 0;
        while(ed->lines[i][j] && pos < MAX_FILE_SIZE - 2) {
            save_buf[pos++] = ed->lines[i][j++];
        }
        if (i < ed->line_count - 1) save_buf[pos++] = '\n';
    }
    save_buf[pos] = 0;
    fs_delete("editor_text");
    int fd = fs_create("editor_text");
    if (fd >= 0) { fs_write(fd, save_buf, pos); fs_close(fd); save_indicator_timer = 50; beep(); }
}

static void editor_draw(editor_t* ed) {
    draw_box_vga(0, 0, 80, 24, VGA_COLOR_WHITE);
    draw_box_vga(0, 0, 80, 1, VGA_COLOR_BLUE);
    set_cursor(2, 0); print_string_color(" BEDI Pro Editor - Dual-Side Scrolling ", CLR_ED_HDR);
    
    if (save_indicator_timer > 0) {
        set_cursor(60, 0); print_string_color(" [ SAVED ] ", (VGA_COLOR_GREEN << 4) | VGA_COLOR_WHITE);
        save_indicator_timer--;
    }

    // Line Number Margin
    draw_box_vga(0, 1, 6, 22, VGA_COLOR_LIGHT_GREY);

    int view_h = 20, view_w = 72;
    for (int i = 0; i < view_h; i++) {
        int l_idx = ed->view_offset_y + i;
        if (l_idx < ed->line_count) {
            // Line Number
            set_cursor(1, 2 + i);
            char num[4]; num[0] = ((l_idx+1)/100)+'0'; num[1] = (((l_idx+1)%100)/10)+'0'; num[2] = ((l_idx+1)%10)+'0'; num[3] = 0;
            print_string_color(num, CLR_ED_NUM);

            // Text Content (with horizontal offset)
            char clipped[80]; int p = 0;
            for (int j = 0; j < view_w && ed->lines[l_idx][ed->view_offset_x + j]; j++) {
                clipped[p++] = ed->lines[l_idx][ed->view_offset_x + j];
            }
            clipped[p] = 0;
            set_cursor(7, 2 + i);
            print_string_color(clipped, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
        }
    }

    if (cursor_blink) {
        int dx = 7 + (ed->cursor_x - ed->view_offset_x);
        int dy = 2 + (ed->cursor_y - ed->view_offset_y);
        if (dx >= 7 && dx < 7 + view_w && dy >= 2 && dy < 2 + view_h) {
            print_char_at(219, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLUE, dx, dy);
        }
    }

    draw_box_vga(0, 23, 80, 1, VGA_COLOR_DARK_GREY);
    set_cursor(2, 23); print_string_color(" CTRL+S: SAVE | ESC: EXIT | NAV: ARROWS ", (VGA_COLOR_DARK_GREY << 4) | VGA_COLOR_WHITE);
}

void editor() {
    init_editor(&current_editor);
    char existing[MAX_FILE_SIZE];
    int bytes = fs_cat("editor_text", existing, MAX_FILE_SIZE);
    if (bytes > 0) {
        int lx = 0, ly = 0;
        for (int i = 0; i < bytes; i++) {
            if (existing[i] == '\n') { ly++; lx = 0; if (ly >= EDITOR_MAX_LINES) break; }
            else { current_editor.lines[ly][lx++] = existing[i]; if (lx >= EDITOR_LINE_LENGTH-1) { ly++; lx = 0; } }
        }
        current_editor.line_count = ly + 1;
    }

    while (1) {
        // Scroll Logic
        if (current_editor.cursor_y < current_editor.view_offset_y) current_editor.view_offset_y = current_editor.cursor_y;
        else if (current_editor.cursor_y >= current_editor.view_offset_y + 20) current_editor.view_offset_y = current_editor.cursor_y - 19;
        
        if (current_editor.cursor_x < current_editor.view_offset_x) current_editor.view_offset_x = current_editor.cursor_x;
        else if (current_editor.cursor_x >= current_editor.view_offset_x + 70) current_editor.view_offset_x = current_editor.cursor_x - 69;

        editor_draw(&current_editor);
        swap_buffers();
        char key = get_key();
        if (key == KEY_ESC) { editor_save(&current_editor); break; }
        if (key != 0) {
            if (key == 19) { editor_save(&current_editor); continue; } 
            if (key == KEY_UP) { if (current_editor.cursor_y > 0) current_editor.cursor_y--; }
            else if (key == KEY_DOWN) { if (current_editor.cursor_y < current_editor.line_count - 1) current_editor.cursor_y++; }
            else if (key == KEY_LEFT) { if (current_editor.cursor_x > 0) current_editor.cursor_x--; }
            else if (key == KEY_RIGHT) { 
                int l = 0; while(current_editor.lines[current_editor.cursor_y][l]) l++;
                if (current_editor.cursor_x < l) current_editor.cursor_x++;
            } else if (key == '\n') {
                if (current_editor.line_count < EDITOR_MAX_LINES) {
                    current_editor.cursor_y++; current_editor.cursor_x = 0;
                    if (current_editor.cursor_y >= current_editor.line_count) current_editor.line_count++;
                }
            } else if (key == '\b') {
                if (current_editor.cursor_x > 0) {
                    current_editor.cursor_x--; int l = current_editor.cursor_x;
                    while(current_editor.lines[current_editor.cursor_y][l]) { current_editor.lines[current_editor.cursor_y][l] = current_editor.lines[current_editor.cursor_y][l+1]; l++; }
                } else if (current_editor.cursor_y > 0) {
                    current_editor.cursor_y--; int l = 0; while(current_editor.lines[current_editor.cursor_y][l]) l++; current_editor.cursor_x = l;
                }
            } else if (key >= 32 && key <= 126) {
                if (current_editor.cursor_x < EDITOR_LINE_LENGTH - 1) {
                    current_editor.lines[current_editor.cursor_y][current_editor.cursor_x++] = key;
                }
            }
            cursor_blink = 1; blink_timer = 0;
        } else { blink_timer++; if (blink_timer > 3000) { cursor_blink = !cursor_blink; blink_timer = 0; } }
    }
}

#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "filesystem.h"

void doc_viewer() {
    char content[MAX_FILE_SIZE];
    int bytes = fs_cat("editor_text", content, MAX_FILE_SIZE);
    
    int page = 0;
    int lines_per_page = 15;
    
    // Split content into lines for slide view
    static char lines[100][80];
    int line_count = 0;
    int curr_x = 0;
    for (int i = 0; i < bytes && line_count < 100; i++) {
        if (content[i] == '\n') {
            lines[line_count][curr_x] = 0;
            line_count++;
            curr_x = 0;
        } else if (curr_x < 79) {
            lines[line_count][curr_x++] = content[i];
        }
    }
    if (curr_x > 0 && line_count < 100) { lines[line_count][curr_x] = 0; line_count++; }

    int total_pages = (line_count + lines_per_page - 1) / lines_per_page;
    if (total_pages == 0) total_pages = 1;

    while (1) {
        restore_background();
        draw_box_vga(5, 2, 70, 20, VGA_COLOR_WHITE);
        draw_box_vga(5, 2, 70, 1, VGA_COLOR_GREEN);
        set_cursor(7, 2); print_string_color(" Document Viewer - SLIDE VIEW MODE ", (VGA_COLOR_GREEN << 4) | VGA_COLOR_WHITE);
        
        // Page Indicator
        set_cursor(60, 2);
        char pg_s[10]; pg_s[0] = (page+1)+'0'; pg_s[1]='/'; pg_s[2] = total_pages+'0'; pg_s[3]=0;
        print_string_color(pg_s, (VGA_COLOR_GREEN << 4) | VGA_COLOR_WHITE);

        if (line_count == 0) {
            set_cursor(10, 10);
            print_string_color("No content to display.", (VGA_COLOR_WHITE << 4) | VGA_COLOR_RED);
        } else {
            for (int i = 0; i < lines_per_page; i++) {
                int l_idx = page * lines_per_page + i;
                if (l_idx < line_count) {
                    set_cursor(7, 4 + i);
                    print_string_color(lines[l_idx], (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
                }
            }
        }
        
        draw_box_vga(5, 22, 70, 1, VGA_COLOR_DARK_GREY);
        set_cursor(7, 22); print_string_color("LEFT/RIGHT: Pages | ESC: Close", (VGA_COLOR_DARK_GREY << 4) | VGA_COLOR_WHITE);
        
        swap_buffers();
        char k = get_key();
        if (k == KEY_ESC) break;
        if (k == KEY_RIGHT && page < total_pages - 1) page++;
        if (k == KEY_LEFT && page > 0) page--;
    }
}

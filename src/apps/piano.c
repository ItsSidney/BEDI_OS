#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/pcspeaker.h"

static void piano_draw_key(int x, int y, int w, int h, int color, const char* label, int pressed) {
    int c = pressed ? VGA_COLOR_BLUE : color;
    draw_box_vga(x, y, w, h, c);
    draw_box_vga(x, y, w, 1, VGA_COLOR_DARK_GREY);
    set_cursor(x + w/2 - 1, y + h - 2);
    print_string_color(label, (c << 4) | (c == VGA_COLOR_WHITE ? VGA_COLOR_BLACK : VGA_COLOR_WHITE));
}

static void piano_render(int last_key) {
    draw_box_vga(5, 4, 70, 16, VGA_COLOR_WHITE);
    draw_box_vga(5, 4, 70, 1, VGA_COLOR_BLUE);
    set_cursor(7, 4); print_string_color(" BEDI Grand Piano - Professional Audio Engine ", (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE);
    
    // Draw White Keys
    const char* labels[] = {"A", "S", "D", "F", "G", "H", "J", "K", "L"};
    for (int i = 0; i < 9; i++) {
        piano_draw_key(8 + i*7, 6, 6, 12, VGA_COLOR_WHITE, labels[i], (last_key == labels[i][0]));
    }
    
    // Draw Black Keys (Accidentals)
    const char* b_labels[] = {"W", "E", " ", "T", "Y", "U"};
    int b_offsets[] = {12, 19, 0, 33, 40, 47};
    for (int i = 0; i < 6; i++) {
        if (b_offsets[i] == 0) continue;
        piano_draw_key(8 + b_offsets[i], 6, 4, 7, VGA_COLOR_BLACK, b_labels[i], (last_key == b_labels[i][0]));
    }
    
    set_cursor(7, 19); print_string_color("Home Row: C4-D5 | Upper Row: Sharps/Flats | ESC: Exit", (VGA_COLOR_WHITE << 4) | VGA_COLOR_DARK_GREY);
}

void piano_app() {
    int last_k = 0;
    while (1) {
        restore_background(); piano_render(last_k); swap_buffers();
        char k = get_key();
        if (k == KEY_ESC) break;
        if (k != 0) {
            last_k = k;
            if (k == 'a' || k == 'A') play_sound(NOTE_C4);
            else if (k == 'w' || k == 'W') play_sound(NOTE_CS4);
            else if (k == 's' || k == 'S') play_sound(NOTE_D4);
            else if (k == 'e' || k == 'E') play_sound(NOTE_DS4);
            else if (k == 'd' || k == 'D') play_sound(NOTE_E4);
            else if (k == 'f' || k == 'F') play_sound(NOTE_F4);
            else if (k == 't' || k == 'T') play_sound(NOTE_FS4);
            else if (k == 'g' || k == 'G') play_sound(NOTE_G4);
            else if (k == 'y' || k == 'Y') play_sound(NOTE_GS4);
            else if (k == 'h' || k == 'H') play_sound(NOTE_A4);
            else if (k == 'u' || k == 'U') play_sound(NOTE_AS4);
            else if (k == 'j' || k == 'J') play_sound(NOTE_B4);
            else if (k == 'k' || k == 'K') play_sound(NOTE_C5);
            
            for(volatile int i=0; i<3000000; i++);
            nosound();
        } else {
            last_k = 0;
        }
    }
}

#include "../../include/guessing_game.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/pcspeaker.h"

#define CLR_G_WIN  ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_G_TIT  ((VGA_COLOR_GREEN << 4) | VGA_COLOR_WHITE)
#define CLR_G_DSP  ((VGA_COLOR_BLACK << 4) | VGA_COLOR_LIGHT_GREEN)

static unsigned int g_seed = 12345;
static unsigned int g_rand() {
    g_seed = (g_seed * 1103515245 + 12345) & 0x7fffffff;
    return g_seed;
}

static void g_render_base() {
    draw_box_vga(10, 3, 60, 18, VGA_COLOR_WHITE);
    draw_box_vga(10, 3, 60, 1, VGA_COLOR_GREEN);
    set_cursor(12, 3); print_string_color(" Number Guessing Elite ", CLR_G_TIT);
    set_cursor(12, 5); print_string_color("I have picked a number (1-100).", CLR_G_WIN);
}

void guessing_game() {
    int target = (g_rand() % 100) + 1;
    int attempts = 0;
    char input[16] = "";
    int input_len = 0;
    char last_msg[64] = "Start guessing!";
    
    while (1) {
        restore_background();
        g_render_base();
        
        draw_box_vga(30, 9, 20, 3, VGA_COLOR_BLACK);
        set_cursor(32, 10); print_string_color("Guess: ", CLR_G_DSP); print_string_color(input, CLR_G_DSP);
        
        set_cursor(12, 13); print_string_color("Attempts: ", CLR_G_WIN);
        char att_s[16]; int t = attempts, si = 0;
        if (t == 0) att_s[si++] = '0';
        else { char r[10]; int rl = 0; while(t > 0) { r[rl++] = (t%10)+'0'; t/=10; } while(rl > 0) att_s[si++] = r[--rl]; }
        att_s[si] = 0; print_string_color(att_s, CLR_G_WIN);

        set_cursor(12, 16); print_string_color("MESSAGE:", (VGA_COLOR_WHITE << 4) | VGA_COLOR_DARK_GREY);
        set_cursor(12, 17); print_string_color(last_msg, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLUE);

        swap_buffers();

        char key = get_key();
        if (key == 27) break;
        if (key == '\n' && input_len > 0) {
            beep();
            int guess = 0; for (int i = 0; i < input_len; i++) guess = guess * 10 + (input[i] - '0');
            attempts++; input[0] = 0; input_len = 0;

            if (guess == target) {
                int won = 1;
                while(won) {
                    restore_background(); g_render_base();
                    set_cursor(14, 17); print_string_color("CONGRATULATIONS! YOU WON!", (VGA_COLOR_WHITE << 4) | VGA_COLOR_GREEN);
                    set_cursor(14, 19); print_string_color("ENTER to Replay | ESC to Exit", CLR_G_WIN);
                    swap_buffers();
                    char k = get_key();
                    if (k == '\n') { target = (g_rand() % 100) + 1; attempts = 0; won = 0; last_msg[0]=0; }
                    if (k == 27) return;
                }
            } else if (guess < target) {
                int m=0; const char* msg = "TOO LOW! Try higher."; while(msg[m]) { last_msg[m]=msg[m]; m++; } last_msg[m]=0;
            } else {
                int m=0; const char* msg = "TOO HIGH! Try lower."; while(msg[m]) { last_msg[m]=msg[m]; m++; } last_msg[m]=0;
            }
        } else if (key >= '0' && key <= '9' && input_len < 3) {
            input[input_len++] = key; input[input_len] = 0;
        } else if (key == '\b' && input_len > 0) {
            input[--input_len] = 0;
        }
    }
}

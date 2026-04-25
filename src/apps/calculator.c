#include "../../include/calculator.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/pcspeaker.h"

#define CLR_CALC_WIN ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_CALC_TIT ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)
#define CLR_CALC_DSP ((VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE)
#define CLR_CALC_BTN ((VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK)
#define CLR_CALC_SEL ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)

static const char* calc_buttons[] = {
    "7", "8", "9", "/", "AC",
    "4", "5", "6", "*", "C",
    "1", "2", "3", "-", " ",
    "0", " ", "=", "+", " "
};

static int calc_row = 0;
static int calc_col = 0;
static char calc_display[32] = "0";
static long calc_operand = 0;
static char calc_op = 0;
static int calc_new_input = 1;

static void calc_draw_btn(int x, int y, const char* label, int selected) {
    if (label[0] == ' ' && label[1] == 0) return;
    int color = selected ? CLR_CALC_SEL : CLR_CALC_BTN;
    draw_box_vga(x, y, 6, 2, (color >> 4) & 0x0F);
    set_cursor(x + 2, y + 1); print_string_color(label, color);
}

static void calc_render() {
    draw_box_vga(10, 2, 60, 20, VGA_COLOR_WHITE);
    draw_box_vga(10, 2, 60, 1, VGA_COLOR_BLUE);
    set_cursor(12, 2); print_string_color(" BEDI Calculator Pro ", CLR_CALC_TIT);

    draw_box_vga(12, 4, 56, 3, VGA_COLOR_BLACK);
    set_cursor(14, 5); print_string_color(calc_display, CLR_CALC_DSP);

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 5; c++) {
            calc_draw_btn(12 + c * 11, 8 + r * 3, calc_buttons[r * 5 + c], (r == calc_row && c == calc_col));
        }
    }
}

static long calc_parse() {
    long res = 0, i = 0, neg = 0;
    if (calc_display[0] == '-') { neg = 1; i++; }
    while(calc_display[i]) {
        if (calc_display[i] >= '0' && calc_display[i] <= '9') res = res * 10 + (calc_display[i] - '0');
        i++;
    }
    return neg ? -res : res;
}

static void calc_format(long val) {
    int i = 0;
    if (val < 0) { calc_display[i++] = '-'; val = -val; }
    if (val == 0) calc_display[i++] = '0';
    else {
        char rev[16]; int rl = 0;
        while(val > 0) { rev[rl++] = (val % 10) + '0'; val /= 10; }
        while(rl > 0) calc_display[i++] = rev[--rl];
    }
    calc_display[i] = 0;
}

void calculator() {
    calc_row = 0; calc_col = 0; calc_display[0] = '0'; calc_display[1] = 0;
    calc_operand = 0; calc_op = 0; calc_new_input = 1;
    while(1) {
        restore_background(); calc_render(); swap_buffers();
        char key = get_key();
        if (key == 0) continue;
        if (key == KEY_ESC) break;
        if (key == KEY_UP) { if (calc_row > 0) calc_row--; }
        else if (key == KEY_DOWN) { if (calc_row < 3) calc_row++; }
        else if (key == KEY_LEFT) { if (calc_col > 0) calc_col--; }
        else if (key == KEY_RIGHT) { if (calc_col < 4) calc_col++; }
        else if (key == '\n') {
            beep();
            const char* lbl = calc_buttons[calc_row * 5 + calc_col];
            if (lbl[0] == ' ' && lbl[1] == 0) continue;
            if (lbl[0] >= '0' && lbl[0] <= '9') {
                if (calc_new_input) { calc_display[0] = lbl[0]; calc_display[1] = 0; calc_new_input = 0; }
                else {
                    int l = 0; while(calc_display[l]) l++;
                    if (l < 30) { calc_display[l] = lbl[0]; calc_display[l+1] = 0; }
                }
            } else if (lbl[0] == 'A') {
                calc_display[0] = '0'; calc_display[1] = 0; calc_operand = 0; calc_op = 0; calc_new_input = 1;
            } else if (lbl[0] == 'C') {
                calc_display[0] = '0'; calc_display[1] = 0; calc_new_input = 1;
            } else if (lbl[0] == '=') {
                if (calc_op) {
                    long val = calc_parse(); long res = 0;
                    if (calc_op == '+') res = calc_operand + val;
                    else if (calc_op == '-') res = calc_operand - val;
                    else if (calc_op == '*') res = calc_operand * val;
                    else if (calc_op == '/') res = (val != 0) ? (calc_operand / val) : 0;
                    calc_format(res); calc_op = 0; calc_new_input = 1;
                }
            } else { calc_operand = calc_parse(); calc_op = lbl[0]; calc_new_input = 1; }
        }
    }
}

#include "apps/calculator.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "gui/gui.h"

// Button categories for color coding
#define BTN_NUM  0
#define BTN_OP   1
#define BTN_EQ   2
#define BTN_CLR  3
#define BTN_FUNC 4

static const char* calc_buttons[] = {
    "C",   "DEL", "%",   "/",
    "7",   "8",   "9",   "*",
    "4",   "5",   "6",   "-",
    "1",   "2",   "3",   "+",
    "+/-", "0",   ".",   "="
};

static const int calc_btn_type[] = {
    BTN_CLR, BTN_FUNC, BTN_FUNC, BTN_OP,
    BTN_NUM, BTN_NUM,  BTN_NUM,  BTN_OP,
    BTN_NUM, BTN_NUM,  BTN_NUM,  BTN_OP,
    BTN_NUM, BTN_NUM,  BTN_NUM,  BTN_OP,
    BTN_FUNC, BTN_NUM, BTN_NUM,  BTN_EQ
};

static char calc_display[32] = "0";
static double calc_operand = 0;
static char calc_op = 0;
static int calc_new_input = 1;
static char calc_history[32] = "";
static int calc_win_id = -1;

static uint32_t btn_bg_color(int type) {
    switch (type) {
        case BTN_OP:   return 0x21262D;
        case BTN_EQ:   return get_accent_color(); 
        case BTN_CLR:  return 0x30363D; 
        case BTN_FUNC: return 0x21262D; 
        case BTN_NUM:
        default:       return 0x0D1117; 
    }
}

static double calc_parse(void) {
    double res = 0.0, div = 1.0;
    int i = 0, neg = 0, decimal = 0;
    if (calc_display[0] == '-') { neg = 1; i++; }
    while (calc_display[i]) {
        if (calc_display[i] == '.') { decimal = 1; }
        else if (calc_display[i] >= '0' && calc_display[i] <= '9') {
            if (decimal) { div *= 10; res += (calc_display[i] - '0') / div; }
            else res = res * 10 + (calc_display[i] - '0');
        }
        i++;
    }
    return neg ? -res : res;
}

static void calc_format(double val) {
    int i = 0;
    if (val < 0) { calc_display[i++] = '-'; val = -val; }
    
    long int_part = (long)val;
    double frac_part = val - int_part;
    
    if (int_part == 0) calc_display[i++] = '0';
    else {
        char rev[16]; int rl = 0;
        while (int_part > 0) { rev[rl++] = (int_part % 10) + '0'; int_part /= 10; }
        while (rl > 0) calc_display[i++] = rev[--rl];
    }
    
    if (frac_part > 0.000001) {
        calc_display[i++] = '.';
        for (int j = 0; j < 6; j++) {
            frac_part *= 10;
            int digit = (int)frac_part;
            calc_display[i++] = digit + '0';
            frac_part -= digit;
        }
        // Trim trailing zeros
        while (calc_display[i-1] == '0') i--;
        if (calc_display[i-1] == '.') i--;
    }
    calc_display[i] = 0;
}

static void calc_on_click(int win_id, int id) {
    const char* lbl = calc_buttons[id];
    if (id == 17) { // 0
        if (calc_new_input) { calc_display[0] = '0'; calc_display[1] = 0; calc_new_input = 0; }
        else {
            int l = gfx_strlen(calc_display);
            if (l < 30) { calc_display[l] = '0'; calc_display[l+1] = 0; }
        }
    } else if (lbl[0] >= '0' && lbl[0] <= '9') {
        if (calc_new_input) { calc_display[0] = lbl[0]; calc_display[1] = 0; calc_new_input = 0; }
        else {
            int l = gfx_strlen(calc_display);
            if (l < 30) { calc_display[l] = lbl[0]; calc_display[l+1] = 0; }
        }
    } else if (lbl[0] == '.') {
        int l = gfx_strlen(calc_display);
        int has_dot = 0; for (int k = 0; k < l; k++) if (calc_display[k] == '.') has_dot = 1;
        if (l < 30 && !has_dot) { calc_display[l] = '.'; calc_display[l + 1] = 0; calc_new_input = 0; }
    } else if (lbl[0] == 'C') {
        calc_display[0] = '0'; calc_display[1] = 0; calc_operand = 0.0; calc_op = 0; calc_new_input = 1;
        calc_history[0] = 0;
    } else if (id == 1) { // DEL
        int l = gfx_strlen(calc_display);
        if (l > 1) { calc_display[l - 1] = 0; } else { calc_display[0] = '0'; calc_display[1] = 0; calc_new_input = 1; }
    } else if (id == 16) { // +/-
        double val = calc_parse();
        calc_format(-val);
        calc_new_input = 0;
    } else if (lbl[0] == '%') {
        double val = calc_parse();
        calc_format(val / 100.0);
        calc_new_input = 1;
    } else if (lbl[0] == '=') {
        if (calc_op) {
            double val = calc_parse(); double res = 0.0;
            if (calc_op == '+') res = calc_operand + val;
            else if (calc_op == '-') res = calc_operand - val;
            else if (calc_op == '*') res = calc_operand * val;
            else if (calc_op == '/') res = (val != 0.0) ? (calc_operand / val) : 0.0;
            
            // Format history
            int hi = 0;
            calc_format(calc_operand);
            while (calc_display[hi] && hi < 20) { calc_history[hi] = calc_display[hi]; hi++; }
            calc_history[hi++] = ' '; calc_history[hi++] = calc_op; calc_history[hi++] = ' ';
            calc_format(val);
            int hj = 0;
            while (calc_display[hj] && hi < 30) { calc_history[hi++] = calc_display[hj++]; }
            calc_history[hi] = 0;
            
            calc_format(res); calc_op = 0; calc_new_input = 1;
        }
    } else {
        calc_operand = calc_parse(); calc_op = lbl[0]; calc_new_input = 1;
        calc_format(calc_operand);
        int hi = 0;
        while (calc_display[hi] && hi < 28) { calc_history[hi] = calc_display[hi]; hi++; }
        calc_history[hi++] = ' '; calc_history[hi++] = calc_op; calc_history[hi] = 0;
    }
}

static void calc_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    uint32_t accent = get_accent_color();
    int disp_h = 100;
    if (h < 200) disp_h = h / 2; // Clamp display height if window is very small
    
    // Background
    gfx_fill_rect(x, y, w, h, 0x010409);
    
    // Display area
    gfx_fill_rect(x + 1, y + 1, w - 2, disp_h - 2, 0x0D1117);
    if (w > 100 && disp_h > 40) {
        gfx_draw_rect_outline(x + 10, y + 10, w - 20, disp_h - 20, 1, 0x30363D);
    }

    // History
    if (calc_history[0] && w > 200) {
        int hist_len = gfx_strlen(calc_history);
        gfx_draw_string_transparent(x + w - 20 - hist_len * 8, y + 15, calc_history, 0x6E7681);
    }
    
    // Main Display
    int len = gfx_strlen(calc_display);
    int text_y = y + disp_h / 2 + 5;
    if (w > len * 8 + 20) {
        gfx_draw_string_transparent(x + w - 20 - len * 8, text_y, calc_display, 0xF0F6FC);
    } else {
        gfx_draw_string_transparent(x + 5, text_y, calc_display, 0xF0F6FC);
    }
    
    if (calc_op) {
        char ops[2] = { calc_op, 0 };
        gfx_draw_string_transparent(x + 20, text_y, ops, accent);
    }
}

static void calc_on_key(int id, char key) {
    if (key >= '0' && key <= '9') {
        if (key == '0') calc_on_click(id, 17);
        else {
            for (int i = 0; i < 20; i++) if (calc_buttons[i][0] == key && calc_buttons[i][1] == 0) { calc_on_click(id, i); return; }
        }
    }
    if (key == '+' || key == '-' || key == '*' || key == '/') {
        for (int i = 0; i < 20; i++) if (calc_buttons[i][0] == key && calc_buttons[i][1] == 0) { calc_on_click(id, i); return; }
    }
    if (key == '\n' || key == '=') calc_on_click(id, 19);
    if (key == '\b') calc_on_click(id, 1);
    if (key == '.') calc_on_click(id, 18);
    if (key == 'c' || key == 'C' || key == 27) calc_on_click(id, 0);
}

static void calc_on_resize(int win_id, int w, int h) {
    wm_clear_buttons(win_id);
    
    int disp_h = 100;
    int body_h = h - WM_TITLEBAR_H;
    if (body_h < 200) disp_h = body_h / 2;

    int pad = 8;
    int grid_y = disp_h + pad;
    int grid_w = w - (pad * 2);
    int grid_h = body_h - disp_h - (pad * 2);
    
    int btn_w = (grid_w - (3 * pad)) / 4;
    int btn_h = (grid_h - (4 * pad)) / 5;
    
    if (btn_w < 10) btn_w = 10;
    if (btn_h < 10) btn_h = 10;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            int bx = pad + c * (btn_w + pad);
            int by = grid_y + r * (btn_h + pad);
            wm_add_button(win_id, idx, bx, by, btn_w, btn_h,
                calc_buttons[idx], btn_bg_color(calc_btn_type[idx]), 0xF0F6FC, calc_on_click);
        }
    }
}

void calculator(void) {
    calc_display[0] = '0'; calc_display[1] = 0;
    calc_operand = 0.0; calc_op = 0; calc_new_input = 1;
    calc_history[0] = 0;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 340, win_h = 500;

    calc_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                                 "Calculator", get_accent_color(), calc_on_render, calc_on_key, calc_on_resize);
    if (calc_win_id < 0) return;

    calc_on_resize(calc_win_id, win_w, win_h);
}

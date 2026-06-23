#include "drivers/video/gfx.h"
#include "drivers/video/gpu.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "gui/gui.h"
#include "kernel/time/timer.h"

#define K_W 36
#define K_H 28
#define GAP 3
#define PAD 8

typedef struct {
    const char* lbl;
    char base;
    char shifted;
    int wmul;
} KeyDef;

static int shift_on, caps_on;
static int prev_mbtn;
static int target_id;
static int osk_win_id;

static int find_target(void) {
    if (target_id >= 0) {
        wm_window_t* w = wm_get_window(target_id);
        if (w && (w->flags & WM_FLAG_VISIBLE) && w->on_key)
            return target_id;
    }
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t* w = wm_get_window_by_index(i);
        if (!w) continue;
        if (w->id == osk_win_id) continue;
        if (w->on_key) {
            target_id = w->id;
            return target_id;
        }
    }
    target_id = -1;
    return -1;
}

static void send_key(char ch) {
    int fid = find_target();
    if (fid < 0) return;
    wm_window_t* w = wm_get_window(fid);
    if (w && w->on_key) w->on_key(fid, ch);
}

static void toggle_shift(void) { shift_on = !shift_on; }
static void toggle_caps(void) { caps_on = !caps_on; }

#define ROWS 5
static KeyDef keys[ROWS][16] = {
    {
        {"`",'`','~',1},{"1",'1','!',1},{"2",'2','@',1},{"3",'3','#',1},
        {"4",'4','$',1},{"5",'5','%',1},{"6",'6','^',1},{"7",'7','&',1},
        {"8",'8','*',1},{"9",'9','(',1},{"0",'0',')',1},{"-",'-','_',1},
        {"=",'=','+',1},{"Bk",'\b','\b',2},
    },
    {
        {"Tab",'\t','\t',2},{"q",'q','Q',1},{"w",'w','W',1},{"e",'e','E',1},
        {"r",'r','R',1},{"t",'t','T',1},{"y",'y','Y',1},{"u",'u','U',1},
        {"i",'i','I',1},{"o",'o','O',1},{"p",'p','P',1},{"[",'[','{',1},
        {"]",']','}',1},{"\\",'\\','|',1},
    },
    {
        {"Cap",0,0,2},{"a",'a','A',1},{"s",'s','S',1},{"d",'d','D',1},
        {"f",'f','F',1},{"g",'g','G',1},{"h",'h','H',1},{"j",'j','J',1},
        {"k",'k','K',1},{"l",'l','L',1},{";",';',':',1},{"'",'\'','"',1},
        {"Ent",'\n','\n',2},
    },
    {
        {"Shf",0,0,2},{"z",'z','Z',1},{"x",'x','X',1},{"c",'c','C',1},
        {"v",'v','V',1},{"b",'b','B',1},{"n",'n','N',1},{"m",'m','M',1},
        {",",',','<',1},{"..",'.','>',1},{"-",'/','?',1},{"Shf",0,0,2},
    },
    {
        {"Ctl",0,0,2},{"Sup",0,0,2},{"Alt",0,0,2},{"Space",' ',' ',8},
        {"Alt",0,0,2},{"Sup",0,0,2},{"Ctl",0,0,2},{"<-",130,130,2},
        {"->",131,131,2},{"/\\",128,128,2},{"\\/",129,129,2},
    },
};

static int row_keys[ROWS] = {14, 14, 13, 12, 11};

static int calc_row_width(int row) {
    int tw = 0;
    for (int c = 0; c < row_keys[row]; c++)
        tw += keys[row][c].wmul * (K_W + GAP);
    return tw - GAP;
}

static int calc_max_width(void) {
    int mw = 0;
    for (int r = 0; r < ROWS; r++) {
        int rw = calc_row_width(r);
        if (rw > mw) mw = rw;
    }
    return mw;
}

static void handle_click(int mx, int my, int wx, int wy, int w, int h) {
    int oy = wy + PAD;
    for (int r = 0; r < ROWS; r++) {
        int rw = calc_row_width(r);
        int rx = wx + (w - rw) / 2;
        for (int c = 0; c < row_keys[r]; c++) {
            KeyDef* k = &keys[r][c];
            int kw = k->wmul * (K_W + GAP) - GAP;
            int kx = rx;
            int ky = oy + r * (K_H + GAP);
            if (mx >= kx && mx < kx + kw && my >= ky && my < ky + K_H) {
                const char* lbl = k->lbl;
                if (lbl[0] == 'B' && lbl[1] == 'k') { send_key('\b'); return; }
                if (lbl[0] == 'E' && lbl[1] == 'n' && lbl[2] == 't') { send_key('\n'); return; }
                if (lbl[0] == 'T' && lbl[1] == 'a' && lbl[2] == 'b') { send_key('\t'); return; }
                if (lbl[0] == 'S' && lbl[1] == 'h' && lbl[2] == 'f') { toggle_shift(); return; }
                if (lbl[0] == 'S' && lbl[1] == 'H' && lbl[2] == 'F') { toggle_shift(); return; }
                if (lbl[0] == 'C' && lbl[1] == 'a' && lbl[2] == 'p') { toggle_caps(); return; }
                if (lbl[0] == 'C' && lbl[1] == 'A' && lbl[2] == 'P') { toggle_caps(); return; }
                if (lbl[0] == 'C' && lbl[1] == 't' && lbl[2] == 'l') return;
                if (lbl[0] == 'C' && lbl[1] == 'T' && lbl[2] == 'L') return;
                if (lbl[0] == 'A' && lbl[1] == 'l' && lbl[2] == 't') return;
                if (lbl[0] == 'A' && lbl[1] == 'L' && lbl[2] == 'T') return;
                if ((lbl[0] == 'S' && lbl[1] == 'u' && lbl[2] == 'p') ||
                    (lbl[0] == 'S' && lbl[1] == 'U' && lbl[2] == 'P')) {
                    gui_toggle_start_menu(); return;
                }
                if (lbl[0] == 'S' && lbl[1] == 'p' && lbl[2] == 'a') { send_key(' '); return; }
                if (lbl[0] == '<' && lbl[1] == '-') { send_key(KEY_LEFT); return; }
                if (lbl[0] == '-' && lbl[1] == '>') { send_key(KEY_RIGHT); return; }
                if (lbl[0] == '/' && lbl[1] == '\\' && lbl[2] == 0) { send_key(KEY_UP); return; }
                if (lbl[0] == '\\' && lbl[1] == '/' && lbl[2] == 0) { send_key(KEY_DOWN); return; }
                char ch = shift_on != caps_on ? k->shifted : k->base;
                send_key(ch);
                shift_on = 0;
                return;
            }
            rx += kw + GAP;
        }
    }
}

static void osk_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy;
    gpu_accel_fill(x, y, w, h, 0x0D0E12);

    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x();
    int my = mouse_get_y();
    if ((mbtn & 1) && !(prev_mbtn & 1)) {
        handle_click(mx, my, x, y, w, h);
    }
    prev_mbtn = mbtn;

    for (int r = 0; r < ROWS; r++) {
        int rw = calc_row_width(r);
        int rx = x + (w - rw) / 2;
        for (int c = 0; c < row_keys[r]; c++) {
            KeyDef* k = &keys[r][c];
            int kw = k->wmul * (K_W + GAP) - GAP;
            int kx = rx;
            int ky = y + PAD + r * (K_H + GAP);
            int hover = (mx >= kx && mx < kx + kw && my >= ky && my < ky + K_H);
            int active = 0;
            const char* lbl = k->lbl;
            char display = k->base;
            if ((lbl[0] == 'S' && lbl[1] == 'h' && lbl[2] == 'f') ||
                (lbl[0] == 'S' && lbl[1] == 'H' && lbl[2] == 'F')) {
                active = shift_on;
            } else if ((lbl[0] == 'C' && lbl[1] == 'a' && lbl[2] == 'p') ||
                       (lbl[0] == 'C' && lbl[1] == 'A' && lbl[2] == 'P')) {
                active = caps_on;
            } else if (shift_on != caps_on && k->shifted) {
                display = k->shifted;
            }
            uint32_t bg = active ? 0x4D5059 : (hover ? 0x262830 : 0x15171D);
            uint32_t border = active ? 0x6D7079 : (hover ? 0x383B44 : 0x1D1F26);
            gfx_fill_rect(kx, ky, kw, K_H, bg);
            gfx_draw_rect_outline(kx, ky, kw, K_H, 1, border);
            int lblen = 0;
            const char* lp = lbl;
            while (*lp) { lblen++; lp++; }
            if (lblen == 1 && k->base) {
                char str[2] = {display, 0};
                gfx_draw_string_transparent(kx + (kw - 8) / 2, ky + 6, str, 0xE4E6EA);
            } else {
                gfx_draw_string_transparent(kx + 4, ky + 6, lbl, 0x94979F);
            }
            rx += kw + GAP;
        }
    }
}

void osk_app(void) {
    shift_on = 0; caps_on = 0; prev_mbtn = 0;
    target_id = wm_get_focused();
    osk_win_id = -1;
    int mw = calc_max_width() + PAD * 2;
    int mh = ROWS * (K_H + GAP) - GAP + PAD * 2 + WM_TITLEBAR_H;
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    osk_win_id = wm_open_window((fw - mw) / 2, fh - mh - 40, mw, mh,
                                "On-Screen Keyboard", 0x6D7079, osk_render, 0, 0);
}

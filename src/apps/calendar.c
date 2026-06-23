#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/mouse.h"
#include "drivers/video/framebuffer.h"
#include "drivers/time/rtc.h"

static int calendar_win_id = -1;
static int win_w = 460, win_h = 400;

static inline int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

// Viewed month/year (separate from today's date for navigation)
static int view_month = 0;
static int view_year  = 0;
static int real_day   = 0; // today's actual day (for highlight)
static int real_month = 0;
static int real_year  = 0;

// Zeller's congruence → 0=Sunday … 6=Saturday
static int get_day_of_week(int d, int m, int y) {
    if (m < 3) { m += 12; y -= 1; }
    int k = y % 100, j = y / 100;
    int h = (d + 13*(m+1)/5 + k + k/4 + j/4 + 5*j) % 7;
    return (h + 6) % 7;
}

static int get_days_in_month(int m, int y) {
    if (m == 2) return ((y%4==0 && y%100!=0)||(y%400==0)) ? 29 : 28;
    if (m==4||m==6||m==9||m==11) return 30;
    return 31;
}

// Draw a two-char string for numbers
static void draw_num(int x, int y, int n, uint32_t c) {
    char buf[3]; buf[0] = (n>9)?(n/10)+'0':' '; buf[1] = (n%10)+'0'; buf[2] = 0;
    gfx_draw_string_transparent(x, y, buf, c);
}

static void draw_calendar_ui(int x, int y, int w, int h) {
    personalization_t* p = get_personalization();
    uint32_t bg        = (p->theme == 0) ? 0x161B22 : 0xF0F4F8;
    uint32_t card_bg   = (p->theme == 0) ? 0x1C2128 : 0xFFFFFF;
    uint32_t text_clr  = (p->theme == 0) ? 0xE8EAED : 0x1F2937;
    uint32_t text_mut  = (p->theme == 0) ? 0x6E7681 : 0x9CA3AF;
    uint32_t border    = (p->theme == 0) ? 0x30363D : 0xE5E7EB;
    uint32_t weekend_c = (p->theme == 0) ? 0x21262D : 0xF9FAFB;
    uint32_t nav_bg    = (p->theme == 0) ? 0x21262D : 0xF3F4F6;
    uint32_t nav_hover = (p->theme == 0) ? 0x30363D : 0xE5E7EB;
    uint32_t accent    = get_accent_color();

    gfx_fill_rect(x, y, w, h, bg);

    // ── Navigation header ──────────────────────────────────────────────────
    int hdr_h = 56;
    gfx_fill_rect(x, y, w, hdr_h, nav_bg);
    gfx_draw_hline(x, y + hdr_h, w, border);

    const char* months[] = {"","January","February","March","April","May","June",
                             "July","August","September","October","November","December"};

    // Month/Year label
    char title[32];
    int ti = 0;
    const char* ms = months[view_month];
    while(*ms) title[ti++] = *ms++;
    title[ti++] = ' ';
    int yr = view_year;
    title[ti++] = (yr/1000)%10+'0';
    title[ti++] = (yr/100)%10+'0';
    title[ti++] = (yr/10)%10+'0';
    title[ti++] =  yr%10+'0';
    title[ti]   = 0;

    // Center title
    int title_w = ti * 8;
    gfx_draw_string_transparent(x + (w - title_w)/2, y + 20, title, accent);

    // "Today" pill (rendered by wm_button, so skip manual draw)
    // Prev / Next (rendered by wm_button, so skip manual draw)

    // ── Day-of-week header ─────────────────────────────────────────────────
    const char* dows[] = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
    int cell_w = (w - 24) / 7;
    int dow_y  = y + hdr_h + 8;
    for (int i = 0; i < 7; i++) {
        uint32_t dc = (i==0||i==6) ? accent : text_mut;
        gfx_draw_string_transparent(x + 12 + i*cell_w + (cell_w-24)/2, dow_y, dows[i], dc);
    }
    gfx_draw_hline(x, dow_y + 16, w, border);

    // ── Day grid ──────────────────────────────────────────────────────────
    int grid_y = dow_y + 22;
    int cell_h = (h - (grid_y - y) - 8) / 6;
    if (cell_h < 30) cell_h = 30;

    int start_dow = get_day_of_week(1, view_month, view_year);
    int days      = get_days_in_month(view_month, view_year);
    int row = 0;

    for (int d = 1; d <= days; d++) {
        int col = (start_dow + d - 1) % 7;
        int px  = x + 12 + col * cell_w;
        int py  = grid_y + row * cell_h;

        int is_today   = (d == real_day && view_month == real_month && view_year == real_year);
        int is_weekend = (col == 0 || col == 6);

        // Cell background
        if (is_today) {
            gfx_fill_rect(px+2, py+1, cell_w-4, cell_h-3, accent);
            gfx_draw_rect_outline(px+2, py+1, cell_w-4, cell_h-3, 1, gfx_lighten(accent, 40));
        } else {
            uint32_t cb = is_weekend ? weekend_c : card_bg;
            gfx_fill_rect(px+2, py+1, cell_w-4, cell_h-3, cb);
            gfx_draw_rect_outline(px+2, py+1, cell_w-4, cell_h-3, 1, border);
        }

        // Number
        uint32_t nc;
        if (is_today)   nc = (p->theme==0) ? 0x111827 : 0xFFFFFF;
        else if (col==0||col==6) nc = accent;
        else            nc = text_clr;
        draw_num(px + (cell_w-16)/2, py + (cell_h-16)/2, d, nc);

        if (col == 6) row++;
    }

    // ── Bottom mini-legend ─────────────────────────────────────────────────
    int leg_y = y + h - 20;
    gfx_fill_rect(x, leg_y, w, 20, nav_bg);
    gfx_draw_hline(x, leg_y, w, border);
    gfx_fill_rect(x+12, leg_y+6, 8, 8, accent);
    gfx_draw_string_transparent(x+24, leg_y+5, "Today", text_mut);
}

static void cal_prev_month(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    view_month--;
    if (view_month < 1) { view_month = 12; view_year--; }
}
static void cal_next_month(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    view_month++;
    if (view_month > 12) { view_month = 1; view_year++; }
}
static void cal_go_today(int win_id, int btn_id) {
    (void)win_id; (void)btn_id;
    view_month = real_month; view_year = real_year;
}

static void calendar_on_render(int win_id, int x, int y, int w, int h, int view_x, int view_y) {
    (void)win_id; (void)view_x; (void)view_y;
    draw_calendar_ui(x, y, w, h);
}

static void calendar_on_key(int win_id, char key) {
    if (key == 27) { wm_close_window(win_id); return; }
    // Arrow left/right to navigate months
    if ((unsigned char)key == 0xCB || key == 'h') cal_prev_month(win_id, 0); // left arrow / h
    if ((unsigned char)key == 0xCD || key == 'l') cal_next_month(win_id, 0); // right arrow / l
    // Home = go to real month
    if (key == 'T' || key == 't') { view_month = real_month; view_year = real_year; }
}

static void setup_calendar_buttons(void) {
    wm_clear_buttons(calendar_win_id);
    personalization_t* p = get_personalization();
    uint32_t btn_bg = (p->theme == 0) ? 0x21262D : 0xF3F4F6;
    uint32_t text_clr = (p->theme == 0) ? 0xE8EAED : 0x1F2937;
    
    int btn_w = 36, btn_h = 28, btn_y = 14;
    int prev_x = 12, next_x = win_w - btn_w - 12;
    int today_w = 60, today_x = (win_w - today_w)/2 - 40;

    wm_add_button(calendar_win_id, 1, prev_x, btn_y, btn_w, btn_h, "<", btn_bg, text_clr, cal_prev_month);
    wm_add_button(calendar_win_id, 2, next_x, btn_y, btn_w, btn_h, ">", btn_bg, text_clr, cal_next_month);
    wm_add_button(calendar_win_id, 3, today_x, btn_y + 4, today_w, 20, "Today", btn_bg, text_clr, cal_go_today);
}

static void calendar_on_resize(int win_id, int w, int h) { 
    (void)win_id; win_w = w; win_h = h;
    setup_calendar_buttons();
}

void calendar_app(void) {
    if (wm_get_window(calendar_win_id)) { wm_bring_to_front(calendar_win_id); return; }

    time_t t; get_time(&t);
    real_day   = t.day;
    real_month = (t.month < 1||t.month > 12) ? 1 : t.month;
    real_year  = t.year;
    view_month = real_month;
    view_year  = real_year;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    calendar_win_id = wm_open_window(
        (fw - win_w)/2, (fh - win_h)/2, win_w, win_h,
        "Calendar", get_accent_color(),
        calendar_on_render, calendar_on_key, calendar_on_resize
    );
    setup_calendar_buttons();
}

#include "gui/gui.h"
#include <stddef.h>
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "drivers/time/rtc.h"
#include "kernel/time/timer.h"
#include "gui/ui.h"
#include "gui/wm.h"
#include "gui/icons.h"
#include "gui/app_icons.h"
#include "kernel/security/security.h"
#include "kernel/mem/kheap.h"
#include "gui/wallpaper_raw.h"
#include "drivers/audio/audio.h"
#include "kernel/lib/string.h"

#define WALLPAPER_CACHE_W 1920
#define WALLPAPER_CACHE_H 1080
static uint32_t wallpaper_cache[WALLPAPER_CACHE_W * WALLPAPER_CACHE_H];
static int wallpaper_cached_w = 0;
static int wallpaper_cached_h = 0;

extern void calculator(void);
extern void file_explorer(void);
extern void terminal_app(void);
extern void pci_scanner_app(void);
extern void system_app(void);
extern void text_editor(void);
extern void cube_3d_app(void);
extern void bdrowser(void);
extern void calendar_app(void);
extern void process_viewer_app(void);
extern void piano_app(void);
extern void snake_app(void);
extern void mines_app(void);
extern void clock_app(void);
extern void tetris_app(void);
extern void pairs_app(void);
extern void osk_app(void);
extern void imgview_app(void);
extern void weather_app(void);
extern void game_2048_app(void);
extern void flappy_app(void);
extern void sudoku_app(void);

#define START_MENU_W 340
#define MENU_ITEM_H 40
#define DESKTOP_COUNT 5
#define APP_DOCK_ICON_SIZE 24
#define APP_DOCK_GAP 4
#define PLUS_BTN_SIZE 24

int gui_running = 1;
static int g_st = 1;
static int m_idx = 0;
static int prev_mouse_btn = 0;

static int all_apps_panel_open = 0;
static int all_apps_scroll = 0;
static int all_apps_hover_idx = -1;
static int desktop_switcher_hover = -1;
static int app_dock_hover_idx = -1;
static int app_dock_right_click_idx = -1;

static char search_buf[64];
static int search_len = 0;
static int search_results[16];
static int search_result_count = 0;
static int search_sel = 0;
static int search_open = 0;

#define TASKBAR_H 30
#define MAX_PINNED_APPS 10
#define MAX_ALL_APPS 23
#define DESKTOP_COUNT 5

typedef struct {
    const char* name;
    void (*launch_func)(void);
    int pinned;
} app_entry_t;

static app_entry_t all_apps[MAX_ALL_APPS] = {
    {"Calculator", calculator, 1},
    {"File Explorer", file_explorer, 1},
    {"Text Editor", text_editor, 1},
    {"Bdrowser", bdrowser, 1},
    {"Calendar", calendar_app, 1},
    {"Process Viewer", process_viewer_app, 0},
    {"Clock", clock_app, 1},
    {"Weather", weather_app, 1},
    {"Piano", piano_app, 0},
    {"Snake", snake_app, 0},
    {"Mines", mines_app, 0},
    {"Tetris", tetris_app, 0},
    {"Pairs", pairs_app, 0},
    {"On-Screen Keyboard", osk_app, 0},
    {"Image Viewer", imgview_app, 0},
    {"PCI Scanner", pci_scanner_app, 0},
    {"System", system_app, 0},
    {"Terminal", terminal_app, 1},
    {"Cube 3D", cube_3d_app, 0},
    {"2048", game_2048_app, 0},
    {"Flappy Bird", flappy_app, 0},
    {"Sudoku", sudoku_app, 0},
    {0, 0, 0}
};

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

static personalization_t prefs = {
    .accent_color_idx = 0,
    .clock_24h = 1,
    .mouse_sensitivity = 2,
    .theme = 0,
    .anim_speed = 2,
    .font_shadow = 1,
    .window_transparency = 230,
    .compact_mode = 0,
    .system_volume = 80,
};

personalization_t* get_personalization(void) { return &prefs; }

static const uint32_t dark_palette[] = {
    0x0D0E12, 0x15171D, 0x1D1F26, 0x262830, 0x383B44, 0x4D5059,
    0xE4E6EA, 0x94979F, 0x6D7079, 0x4A4D56, 0x0B0C10, 0xE4E6EA,
    0x15171D, 0x000000
};

static const uint32_t light_palette[] = {
    0xF8F9FA, 0xFFFFFF, 0xF1F3F4, 0xE8EAED, 0xDADCE0, 0xBDC1C6,
    0x202124, 0x5F6368, 0x80868B, 0x9AA0A6, 0xE8E8E8, 0x1A73E8,
    0xF0F0F0, 0x000000
};

uint32_t get_theme_color(int id) {
    if (prefs.theme == 1) return light_palette[id];
    return dark_palette[id];
}

uint32_t get_accent_color(void) {
    static const uint32_t accent_colors[] = {
        0x8AB4F8, 0x81C995, 0xC58AF9, 0x78D9EC, 0xF28B82, 0xF2CC8C,
        0x669DF6, 0x5BB974, 0xAF5CF7, 0x4ECDC4, 0xE8EAED, 0x9AA0A6
    };
    if ((size_t)prefs.accent_color_idx < sizeof(accent_colors)/sizeof(accent_colors[0]))
        return accent_colors[prefs.accent_color_idx];
    return accent_colors[0];
}

void gui_system_shutdown() {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    gfx_fill_rect(0, 0, fw, fh, 0x000000);
    gfx_draw_string_transparent((fw-150)/2, (fh-16)/2, "SYSTEM SHUTDOWN", 0xFFFFFF);
    swap_buffers();
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
    while(1) { __asm__ volatile("hlt"); }
}

static const char* main_menu[] = { "Applications", "System Tools", "Games", "Accessibility", "Graphics", "Demo", "Terminal", "Shutdown" };
#define MAIN_MENU_COUNT 8
static const char* apps_menu[] = { "Calculator", "File Explorer", "Text Editor", "Bdrowser", "Calendar", "Process Viewer", "Clock", "Weather" };
#define APPS_MENU_COUNT 8
static const char* system_menu[] = { "PCI Scanner", "System" };
#define SYSTEM_MENU_COUNT 2
static const char* games_menu[] = { "Piano", "Snake", "Mines", "Tetris", "Pairs", "2048", "Flappy Bird", "Sudoku" };
#define GAMES_MENU_COUNT 8
static const char* demo_menu[] = { "Teacup" };
#define DEMO_MENU_COUNT 1
static const char* accessibility_menu[] = { "On-Screen Keyboard" };
#define ACCESSIBILITY_MENU_COUNT 1
static const char* graphics_menu[] = { "Image Viewer" };
#define GRAPHICS_MENU_COUNT 1

static const char* all_items[] = { "Calculator", "File Explorer", "PCI Scanner", "System App", "Text Editor", "Teacup", "Terminal", "Process Viewer", "Bdrowser", "Calendar", "Weather", "Piano", "Snake", "Mines", "Clock", "Tetris", "Pairs", "2048", "Flappy Bird", "On-Screen Keyboard", "Image Viewer", "Sudoku", 0 };

void draw_premium_wallpaper() {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    if (fw == 0 || fh == 0) return;

    const unsigned char* rgb = vans_24bit_rgb;
    int iw = VANS_24BIT_RGB_WIDTH;
    int ih = VANS_24BIT_RGB_HEIGHT;
    uint32_t stride = gfx_get_stride();
    uint32_t* bb = gfx_get_back_buffer();

    int draw_h = (int)fh;
    int bot_offset = TASKBAR_H;
    int avail = draw_h - bot_offset;
    if (avail < 10) avail = 10;

    if ((int)fw != wallpaper_cached_w || (int)fh != wallpaper_cached_h) {
        wallpaper_cached_w = fw;
        wallpaper_cached_h = fh;
        for (int row = 0; row < avail; row++) {
            uint32_t* dst_row = wallpaper_cache + row * WALLPAPER_CACHE_W;
            int sy = (row * ih) / avail;
            if (sy < 0) sy = 0;
            if (sy >= ih) sy = ih - 1;
            for (int col = 0; col < (int)fw; col++) {
                int sx = (col * iw) / (int)fw;
                if (sx < 0) sx = 0;
                if (sx >= iw) sx = iw - 1;
                int idx = (sy * iw + sx) * 3;
                uint32_t r = rgb[idx];
                uint32_t g = rgb[idx + 1];
                uint32_t b = rgb[idx + 2];
                dst_row[col] = gfx_rgb_to_pixel(RGB(r, g, b));
            }
        }
    }

    for (int row = 0; row < avail; row++) {
        memcpy(bb + row * stride, wallpaper_cache + row * WALLPAPER_CACHE_W, fw * sizeof(uint32_t));
    }
}

void draw_taskbar() {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    if (fw == 0 || fh == 0) return;
    int taskbar_h = TASKBAR_H, taskbar_y = fh - taskbar_h;
    personalization_t* p = get_personalization();
    uint32_t bg_clr = (p->theme == 0) ? 0x0F1115 : 0xF3F4F6;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x1F2937;
    uint32_t muted = (p->theme == 0) ? 0x6D7079 : 0x9CA3AF;
    uint32_t border = (p->theme == 0) ? 0x2A2D35 : 0xD1D5DB;
    uint32_t tab_active = (p->theme == 0) ? 0x2C303A : 0xE5E7EB;
    uint32_t tab_inactive = (p->theme == 0) ? 0x1A1D24 : 0xEAECF0;
    uint32_t tray_bg = (p->theme == 0) ? 0x0A0C10 : 0xEAEBEE;
    int mx = mouse_get_x(), my = mouse_get_y();
    int margin = 8, gap = 4;

    /* Taskbar background */
    gfx_fill_rect(0, taskbar_y, fw, taskbar_h, bg_clr);
    gfx_draw_hline(0, taskbar_y, fw, border);

    /* Left: Start button */
    int start_btn_w = 48;
    int start_x = margin;
    int start_hover = point_in_rect(mx, my, start_x, taskbar_y + 4, start_btn_w, taskbar_h - 8);
    if (start_hover || g_st >= 2) {
        gfx_fill_rect_rounded(start_x, taskbar_y + 4, start_btn_w, taskbar_h - 8, 6, (p->theme == 0) ? 0x1F2229 : 0xE5E7EB);
        gfx_draw_rect_rounded_outline(start_x, taskbar_y + 4, start_btn_w, taskbar_h - 8, 6, 1, border);
    } else {
        gfx_draw_rect_rounded_outline(start_x, taskbar_y + 4, start_btn_w, taskbar_h - 8, 6, 1, border);
    }
    draw_start_icon(start_x + 8, taskbar_y + 8, start_btn_w - 16, taskbar_h - 16);

    /* Search */
    int search_btn_w = 36;
    int search_x = start_x + start_btn_w + gap;
    int search_hover = point_in_rect(mx, my, search_x, taskbar_y + 4, search_btn_w, taskbar_h - 8);
    if (search_hover || search_open) {
        gfx_fill_rect_rounded(search_x, taskbar_y + 4, search_btn_w, taskbar_h - 8, 6, (p->theme == 0) ? 0x1F2229 : 0xE5E7EB);
        gfx_draw_rect_rounded_outline(search_x, taskbar_y + 4, search_btn_w, taskbar_h - 8, 6, 1, border);
    } else {
        gfx_draw_rect_rounded_outline(search_x, taskbar_y + 4, search_btn_w, taskbar_h - 8, 6, 1, border);
    }
    draw_search_icon(search_x + 6, taskbar_y + 8, search_btn_w - 12, taskbar_h - 16);

    /* Separator after search */
    int sep1_x = search_x + search_btn_w + gap / 2;
    gfx_draw_vline(sep1_x, taskbar_y + 8, taskbar_h - 16, border);

    /* Middle: window tabs */
    int tab_area_x = sep1_x + gap / 2;
    int right_reserved = 150 + 8 + 120 + 8; /* tray + gap + desktop switcher + gap */
    int tab_area_w = fw - tab_area_x - right_reserved;
    if (tab_area_w < 100) tab_area_w = 100;

    int open_windows = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm_get_window_by_index(i)) open_windows++;
    }

    int tab_w = 120;
    if (open_windows > 0) {
        int max_tab_w = (tab_area_w - (open_windows - 1) * (gap + 2)) / open_windows;
        if (max_tab_w < 80) max_tab_w = 80;
        tab_w = max_tab_w;
    }

    int wx = tab_area_x;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t* win = wm_get_window_by_index(i);
        if (!win) continue;
        int is_focused = (win->flags & WM_FLAG_FOCUSED) != 0;
        int tab_h = taskbar_h - 6;
        int tab_y = taskbar_y + 3;
        int tab_hover = point_in_rect(mx, my, wx, tab_y, tab_w, tab_h);
        uint32_t tab_bg = is_focused ? tab_active : (tab_hover ? border : tab_inactive);
        uint32_t tab_text = is_focused ? text_clr : muted;

        gfx_fill_rect_rounded(wx, tab_y, tab_w, tab_h, 6, tab_bg);
        gfx_draw_rect_rounded_outline(wx, tab_y, tab_w, tab_h, 6, 1, border);

        char short_title[20];
        int j = 0;
        while (win->title[j] && j < 16) { short_title[j] = win->title[j]; j++; }
        short_title[j] = 0;
        int tw = gfx_strlen(short_title) * 8;
        if (tw > tab_w - 16) tw = tab_w - 16;
        gfx_draw_string_transparent(wx + (tab_w - tw) / 2, tab_y + (tab_h - 12) / 2, short_title, tab_text);
        wx += tab_w + gap + 2;
    }

    /* Desktop switcher (popup above taskbar, anchored to right tray area) */
    int desk_w = 28, desk_h = 22, desk_gap = 4;
    int desk_area_w = DESKTOP_COUNT * (desk_w + desk_gap) - desk_gap;
    int desk_area_x = fw - 150 - margin - 8 - desk_area_w;
    int desk_start_x = desk_area_x;
    for (int i = 0; i < DESKTOP_COUNT; i++) {
        int dx = desk_start_x + i * (desk_w + desk_gap);
        int dy = taskbar_y - desk_h - 2;
        int hover = point_in_rect(mx, my, dx, dy, desk_w, desk_h);
        int is_current = (i == wm_get_current_desktop());
        uint32_t box_bg = is_current ? tab_active : (hover ? border : (p->theme == 0 ? 0x16181E : 0xE5E7EB));
        uint32_t txt_clr = is_current ? text_clr : muted;
        gfx_fill_rect_rounded(dx, dy, desk_w, desk_h, 4, box_bg);
        gfx_draw_rect_rounded_outline(dx, dy, desk_w, desk_h, 4, 1, border);
        char num[2];
        num[0] = '1' + i;
        num[1] = 0;
        gfx_draw_string_transparent(dx + 8, dy + 3, num, txt_clr);
    }

    /* Right: volume grid square + clock tray */
    int tray_w = 80;
    int tray_x = fw - tray_w - margin;
    gfx_fill_rect(tray_x, taskbar_y, tray_w, taskbar_h, tray_bg);

    int vol = audio_get_master_volume();
    int vol_x = tray_x + 10;
    int vol_y = taskbar_y + (taskbar_h - 14) / 2;
    int mbtn = mouse_get_buttons();
    uint32_t vol_clr = audio_is_muted() ? 0xEF4444 : text_clr;
    uint32_t vol_empty = (p->theme == 0) ? 0x4D5059 : 0xBDC1C6;

    /* Small 14x14 grid square */
    int gx = vol_x, gy = vol_y, gw = 14, gh = 14;
    gfx_draw_rect_outline(gx, gy, gw, gh, 1, border);
    int cell = 4, cg = 1;
    int filled = (vol * 9 + 50) / 100;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            int idx = r * 3 + c;
            int cx = gx + 2 + c * (cell + cg);
            int cy = gy + 2 + (2 - r) * (cell + cg); // fill from bottom
            if (idx < filled) {
                gfx_fill_rect(cx, cy, cell, cell, vol_clr);
            } else {
                gfx_fill_rect(cx, cy, cell, cell, vol_empty);
            }
        }
    }
    int vol_hover = point_in_rect(mx, my, gx, gy, gw, gh);
    if (vol_hover && (mbtn & 1)) {
        int rel = mx - gx;
        if (rel < 0) rel = 0;
        if (rel > gw) rel = gw;
        audio_set_master_volume((rel * 100) / gw);
    }

    /* Clock */
    time_t t; get_time(&t);
    char t_str[12];
    if (p->clock_24h) {
        t_str[0] = (t.hour / 10) + '0'; t_str[1] = (t.hour % 10) + '0';
        t_str[2] = ':';
        t_str[3] = (t.minute / 10) + '0'; t_str[4] = (t.minute % 10) + '0';
        t_str[5] = 0;
    } else {
        int h12 = t.hour % 12; if (h12 == 0) h12 = 12;
        t_str[0] = (h12 / 10) + '0'; t_str[1] = (h12 % 10) + '0';
        t_str[2] = ':';
        t_str[3] = (t.minute / 10) + '0'; t_str[4] = (t.minute % 10) + '0';
        t_str[5] = ' '; t_str[6] = (t.hour < 12) ? 'A' : 'P'; t_str[7] = 'M'; t_str[8] = 0;
    }
    int clock_w = gfx_strlen(t_str) * 8;
    gfx_draw_string_transparent(tray_x + tray_w - clock_w - 6, taskbar_y + 12, t_str, text_clr);
}

static void get_menu_geometry(int* mx, int* my, int* mw, int* mh, int* ic) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    *mw = 340; *mx = 8;
    int header_h = 56, footer_h = 6;
    if (g_st == 2) *ic = MAIN_MENU_COUNT;
    else if (g_st == 3) *ic = APPS_MENU_COUNT;
    else if (g_st == 5) *ic = SYSTEM_MENU_COUNT;
    else if (g_st == 6) *ic = DEMO_MENU_COUNT;
    else if (g_st == 7) *ic = GAMES_MENU_COUNT;
    else if (g_st == 9) *ic = ACCESSIBILITY_MENU_COUNT;
    else if (g_st == 11) *ic = GRAPHICS_MENU_COUNT;
    else *ic = 0;

    int content_h = *ic * (MENU_ITEM_H - 4);
    *mh = content_h + header_h + footer_h;
    if (*mh < 260) *mh = 260;
    *my = fh - TASKBAR_H - *mh - 6;
}

static void render_menu() {
    int mx, my, mw, mh, ic; get_menu_geometry(&mx, &my, &mw, &mh, &ic);
    personalization_t* p = get_personalization();
    uint32_t bg_main = (p->theme == 0) ? 0x131519 : 0xF9FAFB;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x111827;
    uint32_t head_bg  = (p->theme == 0) ? 0x1A1D24 : 0xFFFFFF;
    uint32_t sel_bg   = (p->theme == 0) ? 0x252830 : 0xF3F4F6;
    uint32_t muted    = (p->theme == 0) ? 0x8B8F99 : 0x6B7280;
    uint32_t border   = (p->theme == 0) ? 0x2C303A : 0xE5E7EB;
    uint32_t accent   = get_accent_color();
    int mx_mouse = mouse_get_x(), my_mouse = mouse_get_y();

    /* Outer panel */
    gfx_draw_shadow(mx, my, mw, mh, 20);
    gfx_fill_rect(mx, my, mw, mh, bg_main);
    gfx_draw_rect_outline(mx, my, mw, mh, 1, border);

    /* Header */
    int header_h = 52;
    gfx_fill_rect(mx + 1, my + 1, mw - 2, header_h, head_bg);
    gfx_draw_hline(mx + 16, my + header_h, mw - 32, border);

    /* Logo / back indicator in header */
    if (g_st == 2) {
        draw_start_icon(mx + 10, my + 10, 28, 28);
        gfx_draw_string_transparent(mx + 46, my + 18, "BEDI OS", text_clr);
    } else {
        /* Back button */
        int back_x = mx + 8, back_y = my + 14, back_w = 28, back_h = 24;
        int back_hover = point_in_rect(mx_mouse, my_mouse, back_x, back_y, back_w, back_h);
        if (back_hover) {
            gfx_fill_rect(back_x, back_y, back_w, back_h, (p->theme == 0) ? 0x262E38 : 0xE5E7EB);
        }
        gfx_draw_rect_outline(back_x, back_y, back_w, back_h, 1, border);
        gfx_draw_string_transparent(back_x + 8, back_y + 6, "<", text_clr);

        const char* sub = (g_st == 3) ? "Applications" : (g_st == 5) ? "System Tools" : (g_st == 7) ? "Games" : (g_st == 9) ? "Accessibility" : (g_st == 11) ? "Graphics" : "Demo";
        gfx_draw_string_transparent(mx + 44, my + 18, sub, text_clr);
    }

    const char** items = (g_st == 2) ? main_menu : (g_st == 3) ? apps_menu : (g_st == 5) ? system_menu : (g_st == 7) ? games_menu : (g_st == 9) ? accessibility_menu : (g_st == 11) ? graphics_menu : demo_menu;
    int has_arrow_at = (g_st == 2) ? 6 : -1;

    for (int i = 0; i < ic; i++) {
        int iy = my + header_h + i * (MENU_ITEM_H - 4);
        int is_sel = (m_idx == i);
        int row_h = MENU_ITEM_H - 4;

        /* Selection / hover pill */
        uint32_t pill_bg = is_sel ? sel_bg : ((mx_mouse >= mx + 8 && mx_mouse <= mx + mw - 8 && my_mouse >= iy && my_mouse <= iy + row_h) ? (p->theme == 0 ? 0x1D212A : 0xE5E7EB) : 0);
        if (pill_bg != 0) {
            gfx_fill_rect(mx + 6, iy, mw - 12, row_h, pill_bg);
        }

        /* Icon + label row */
        int icon_sz = 20;
        int icon_x = mx + 16;
        int icon_y = iy + (row_h - icon_sz) / 2;
        draw_app_icon(items[i], icon_x, icon_y);

        gfx_draw_string_transparent(mx + 46, iy + row_h / 2 - 6, items[i], is_sel ? text_clr : muted);

        /* Arrow badge for submenu rows */
        if (i < has_arrow_at) {
            gfx_draw_string_transparent(mx + mw - 22, iy + row_h / 2 - 6, ">", muted);
        }
    }
}

static int search_scroll = 0;

static void update_search_results(void) {
    search_result_count = 0;
    if (search_len == 0) {
        for (int i = 0; all_items[i] && search_result_count < 32; i++)
            search_results[search_result_count++] = i;
    } else {
        for (int i = 0; all_items[i] && search_result_count < 32; i++) {
            const char *h = all_items[i], *n = search_buf; int m = 1;
            while(*n) {
                char a=*h, b=*n;
                if(a>='A'&&a<='Z')a+=32; if(b>='A'&&b<='Z')b+=32;
                if(a!=b){m=0;break;} h++;n++;
            }
            if(m) search_results[search_result_count++] = i;
        }
    }
    search_sel = 0; search_scroll = 0;
}

static void render_search_panel(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int mw = 600, mh = 400;
    int mx = (fw - mw) / 2;
    int my = (fh - mh) / 2 - 40; // Slightly above center for Spotlight feel
    personalization_t* p = get_personalization();
    uint32_t bg_main = (p->theme == 0) ? 0x15171D : 0xFFFFFF;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x202124;
    uint32_t head_bg = (p->theme == 0) ? 0x1D1F26 : 0xF8F9FA;
    uint32_t accent = get_accent_color();

    // Spotlight shadow
    gfx_draw_shadow(mx, my, mw, mh, 40);
    gfx_fill_rect(mx, my, mw, mh, bg_main);
    gfx_draw_rect_outline(mx, my, mw, mh, 1, (p->theme == 0) ? 0x383B44 : 0xDADCE0);

    // Big Search Input Area
    int input_h = 70;
    gfx_fill_rect(mx + 2, my + 2, mw - 4, input_h, head_bg);
    gfx_draw_hline(mx, my + input_h + 2, mw, (p->theme == 0) ? 0x383B44 : 0xDADCE0);

    gfx_draw_string_transparent(mx + 20, my + 28, "Q", text_clr); // Magnifying glass stand-in

    char display_buf[64];
    int start_pos = (search_len > 50) ? search_len - 50 : 0;
    int k = 0;
    while(k < 50 && search_buf[start_pos + k]) { display_buf[k] = search_buf[start_pos + k]; k++; }
    display_buf[k] = 0;

    gfx_draw_string_transparent(mx + 60, my + 28, display_buf, text_clr);
    
    // Blinking Cursor
    if ((timer_get_ms() / 500) % 2 == 0) {
        int cx = mx + 60 + k * 8;
        if (cx < mx + mw - 20) gfx_fill_rect(cx, my + 26, 2, 16, text_clr);
    }

    if (search_len == 0 && search_result_count == 0) {
        gfx_draw_string_transparent(mx + 60, my + 28, "Type to search...", (p->theme == 0) ? 0x6D7079 : 0x9AA0A6);
    }

    int ry = my + input_h + 10;
    int show_count = 6;
    if (search_sel >= search_scroll + show_count) search_scroll = search_sel - show_count + 1;
    if (search_sel < search_scroll) search_scroll = search_sel;

    gfx_push_clip(mx + 4, ry, mw - 8, mh - (ry - my) - 4);
    for (int i = 0; i < show_count && (i + search_scroll) < search_result_count; i++) {
        int idx = i + search_scroll;
        int iy = ry + i * 48; // Taller rows
        if (search_sel == idx) {
            gfx_fill_rect(mx + 10, iy, mw - 20, 44, (p->theme == 0 ? 0x262830 : 0xE8EAED));
            gfx_draw_rect_outline(mx + 10, iy, mw - 20, 44, 1, text_clr);
        }
        draw_app_icon(all_items[search_results[idx]], mx + 24, iy + 10);
        gfx_draw_string_transparent(mx + 64, iy + 16, all_items[search_results[idx]], text_clr);
    }
    gfx_pop_clip();

    if (search_result_count > show_count) {
        int sb_h = mh - (ry - my) - 20;
        int th = (sb_h * show_count) / search_result_count;
        int ty = ry + (search_scroll * (sb_h - th)) / (search_result_count - show_count);
        gfx_fill_rect(mx + mw - 10, ry, 4, sb_h, (p->theme == 0 ? 0x262830 : 0xE8EAED));
        gfx_fill_rect(mx + mw - 10, ty, 4, th, text_clr);
    }
}

static void launch_item(int global_idx) {
    g_st = 1; search_open = 0;
    if      (global_idx == 0) calculator();
    else if (global_idx == 1) file_explorer();
    else if (global_idx == 2) pci_scanner_app();
    else if (global_idx == 3) system_app();
    else if (global_idx == 4) text_editor();
    else if (global_idx == 5) cube_3d_app();
    else if (global_idx == 6) terminal_app();
    else if (global_idx == 7) process_viewer_app();
    else if (global_idx == 8) bdrowser();
    else if (global_idx == 9) calendar_app();
    else if (global_idx == 10) weather_app();
    else if (global_idx == 11) piano_app();
    else if (global_idx == 12) snake_app();
    else if (global_idx == 13) mines_app();
    else if (global_idx == 14) clock_app();
    else if (global_idx == 15) tetris_app();
    else if (global_idx == 16) pairs_app();
    else if (global_idx == 17) game_2048_app();
    else if (global_idx == 18) flappy_app();
    else if (global_idx == 19) osk_app();
    else if (global_idx == 20) imgview_app();
    else if (global_idx == 21) sudoku_app();
}

static void handle_menu_click(int cx, int cy) {
    int mx, my, mw, mh, ic; get_menu_geometry(&mx, &my, &mw, &mh, &ic);
    if (!point_in_rect(cx, cy, mx, my, mw, mh)) { g_st = 1; return; }

    // Back button click (header area on sub-menus)
    if (g_st > 2 && cy < my + 52) { g_st = 2; m_idx = 0; return; }

    int rel_y = cy - (my + 52);
    if (rel_y < 0) return;
    int idx = rel_y / (MENU_ITEM_H - 4);

    if (idx >= 0 && idx < ic) {
        if (g_st == 2) {
            if      (idx == 0) { g_st = 3; m_idx = 0; }
            else if (idx == 1) { g_st = 5; m_idx = 0; }
            else if (idx == 2) { g_st = 7; m_idx = 0; }
            else if (idx == 3) { g_st = 9; m_idx = 0; }
            else if (idx == 4) { g_st = 11; m_idx = 0; }
            else if (idx == 5) { g_st = 6; m_idx = 0; }
            else if (idx == 6) { g_st = 1; terminal_app(); }
            else if (idx == 7) { g_st = 1; gui_system_shutdown(); }
        } else if (g_st == 9) {
            g_st = 1;
            if (idx == 0) osk_app();
        } else if (g_st == 11) {
            g_st = 1;
            if (idx == 0) imgview_app();
        } else if (g_st == 3) {
            g_st = 1;
            if      (idx == 0) calculator();
            else if (idx == 1) file_explorer();
            else if (idx == 2) text_editor();
            else if (idx == 3) bdrowser();
            else if (idx == 4) calendar_app();
            else if (idx == 5) process_viewer_app();
            else if (idx == 6) clock_app();
            else if (idx == 7) weather_app();
        } else if (g_st == 5) {
            g_st = 1;
            if      (idx == 0) pci_scanner_app();
            else if (idx == 1) system_app();
        } else if (g_st == 6) {
            g_st = 1;
            if (idx == 0) cube_3d_app();
        } else if (g_st == 7) {
            g_st = 1;
            if      (idx == 0) piano_app();
            else if (idx == 1) snake_app();
            else if (idx == 2) mines_app();
            else if (idx == 3) tetris_app();
            else if (idx == 4) pairs_app();
            else if (idx == 5) game_2048_app();
            else if (idx == 6) flappy_app();
            else if (idx == 7) sudoku_app();
        }
    }
}

void gui_handle_menu_key(char key_in) {
    unsigned char key = (unsigned char)key_in; if (g_st < 2) return;
    int mx, my, mw, mh, ic; get_menu_geometry(&mx, &my, &mw, &mh, &ic);
    if (key == 27) { if (g_st > 2) { g_st = 2; m_idx = 0; } else g_st = 1; }
    else if (KEY_MATCH(key, KEY_UP)) { if (m_idx > 0) m_idx--; }
    else if (KEY_MATCH(key, KEY_DOWN)) { if (m_idx < ic - 1) m_idx++; }
    else if (KEY_MATCH(key, KEY_LEFT)) { if (g_st > 2) { g_st = 2; m_idx = 0; } }
    else if (KEY_MATCH(key, KEY_RIGHT) || key == '\n') {
        if (g_st == 2 && m_idx < 6) {
            if      (m_idx == 0) g_st = 3;
            else if (m_idx == 1) g_st = 5;
            else if (m_idx == 2) g_st = 7;
            else if (m_idx == 3) g_st = 9;
            else if (m_idx == 4) g_st = 11;
            else if (m_idx == 5) g_st = 6;
            m_idx = 0;
        }
        else if (key == '\n') handle_menu_click(mx + 8, my + 52 + m_idx * (MENU_ITEM_H - 4) + ((MENU_ITEM_H - 4) / 2));
    }
}

void gui_toggle_start_menu(void) { search_open = 0; if (g_st >= 2) g_st = 1; else g_st = 2; m_idx = 0; }
void gui_open_search(void) { g_st = 1; search_open = 1; search_len = 0; search_buf[0] = 0; search_sel = 0; update_search_results(); }
void gui_toggle_search(void) { if (search_open) search_open = 0; else gui_open_search(); }
int gui_is_menu_open(void) { return g_st >= 2; }
int gui_is_search_open(void) { return search_open; }
int gui_is_topbar_cfg_open(void) { return 0; }
void gui_handle_topbar_cfg_key(char key) { (void)key; }

void gui_handle_search_key(char key_in) {
    unsigned char key = (unsigned char)key_in; if (!search_open) return;
    if (key == 27) { search_open = 0; return; }
    if (key == '\b') { if (search_len > 0) { search_buf[--search_len] = 0; update_search_results(); } }
    else if (KEY_MATCH(key, KEY_UP)) { if (search_sel > 0) search_sel--; }
    else if (KEY_MATCH(key, KEY_DOWN)) { if (search_sel < search_result_count - 1) search_sel++; }
    else if (key == '\n' && search_result_count > 0) launch_item(search_results[search_sel]);
    else if (key >= 32 && key <= 126 && search_len < 62) { search_buf[search_len++] = (char)key; search_buf[search_len] = 0; update_search_results(); }
}

void start_gui(void) {
    gui_running = 1; g_st = 1; search_open = 0; m_idx = 0; wm_init(); gfx_reset_clip();
    while (gui_running) {
        int mbtn = mouse_get_buttons(), cx = mouse_get_x(), cy = mouse_get_y(), fw = get_fb_width(), fh = get_fb_height();
        if (fw == 0) { sleep_ms(10); continue; }
        int clicked = (mbtn & 1) && !(prev_mouse_btn & 1), taskbar_y = fh - TASKBAR_H, click_handled = 0;
        int margin = 8, gap = 4;
        int start_btn_w = 48, search_btn_w = 36;
        int search_x = margin + start_btn_w + gap;
        int sep1_x = search_x + search_btn_w + gap / 2;

        /* Compute tab area */
        int tab_area_x = sep1_x + gap / 2;
        int tab_area_w = fw - tab_area_x - 220;
        if (tab_area_w < 100) tab_area_w = 100;

        int open_windows = 0;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (wm_get_window_by_index(i)) open_windows++;
        }
        int tab_w = 120;
        if (open_windows > 0) {
            int max_tab = (tab_area_w - (open_windows - 1) * (gap + 2)) / open_windows;
            if (max_tab < 80) max_tab = 80;
            tab_w = max_tab;
        }
        int tab_end_x = tab_area_x + open_windows * (tab_w + gap + 2) - 2;

        /* Desktop switcher bounds */
        int desk_w = 24, desk_h = 28, desk_gap = 4;
        int desk_area_w = DESKTOP_COUNT * (desk_w + desk_gap) - desk_gap;
        int desk_area_x = fw - 150 - margin - 8 - desk_area_w;
        if (desk_area_x < tab_end_x + 16) desk_area_x = tab_end_x + 16;
        int desk_start_x = desk_area_x;

        if (clicked) {
            /* Start button */
            if (!click_handled && point_in_rect(cx, cy, margin, taskbar_y, start_btn_w, TASKBAR_H)) {
                search_open = 0; if (g_st >= 2) g_st = 1; else g_st = 2; m_idx = 0; click_handled = 1;
            }

            /* Search button */
            if (!click_handled && point_in_rect(cx, cy, search_x, taskbar_y, search_btn_w, TASKBAR_H)) {
                g_st = 1; gui_toggle_search(); click_handled = 1;
            }

            /* Window tabs */
            if (!click_handled) {
                int wx = tab_area_x;
                int tab_h = TASKBAR_H - 6;
                int tab_y = taskbar_y + 3;
                for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                    wm_window_t* win = wm_get_window_by_index(i);
                    if (!win) continue;
                    if (point_in_rect(cx, cy, wx, tab_y, tab_w, tab_h)) {
                        wm_bring_to_front(win->id);
                        click_handled = 1;
                        break;
                    }
                    wx += tab_w + gap + 2;
                }
            }

            /* Desktop switcher */
            if (!click_handled) {
                for (int i = 0; i < DESKTOP_COUNT; i++) {
                    int dx = desk_start_x + i * (desk_w + desk_gap);
                    int dy = taskbar_y - desk_h - 2;
                    if (point_in_rect(cx, cy, dx, dy, desk_w, desk_h)) {
                        wm_set_current_desktop(i);
                        click_handled = 1;
                        break;
                    }
                }
            }

            /* Volume tray */
            if (!click_handled) {
                int vol_tray_x = fw - 80 - margin;
                int vol_x = vol_tray_x + 10;
                int vol_y = taskbar_y + (TASKBAR_H - 14) / 2;
                if (point_in_rect(cx, cy, vol_x, vol_y, 14, 14)) {
                    int rel = cx - vol_x;
                    if (rel < 0) rel = 0;
                    if (rel > 14) rel = 14;
                    audio_set_master_volume((rel * 100) / 14);
                    click_handled = 1;
                }
            }

            if (!click_handled && g_st >= 2) {
                int mmx, mmy, mmw, mmh, ic; get_menu_geometry(&mmx, &mmy, &mmw, &mmh, &ic);
                if (point_in_rect(cx, cy, mmx, mmy, mmw, mmh)) handle_menu_click(cx, cy); else g_st = 1;
                click_handled = 1;
            }
            if (!click_handled && search_open) {
                uint32_t fw = get_fb_width(), fh = get_fb_height();
                int mw = 600, mh = 400;
                int mx = (fw - mw) / 2, my = (fh - mh) / 2 - 40;
                if (point_in_rect(cx, cy, mx, my, mw, mh)) {
                    int ry = my + 80;
                    int rel_y = cy - ry;
                    if (rel_y >= 0) { 
                        int idx = rel_y / 48; 
                        int limit = (search_result_count < 6) ? search_result_count : 6;
                        if (idx < limit && (idx + search_scroll) < search_result_count) {
                            launch_item(search_results[idx + search_scroll]); 
                        }
                    }
                } else search_open = 0;
                click_handled = 1;
            }
            if (!click_handled) { g_st = 1; search_open = 0; }
        }

        if (g_st >= 2) {
            int mmx, mmy, mmw, mmh, ic; get_menu_geometry(&mmx, &mmy, &mmw, &mmh, &ic);
            if(point_in_rect(cx, cy, mmx, mmy, mmw, mmh)) {
                int ry = cy - (mmy + 52); if(ry >= 0) { int i = ry / (MENU_ITEM_H - 4); if(i < ic) m_idx = i; }
            }
        }
        draw_premium_wallpaper(); wm_tick();
        audio_update(); audio_hardware_sync();
        draw_taskbar();
        if (g_st >= 2) render_menu(); if (search_open) render_search_panel();
        mouse_draw_cursor(); swap_buffers(); prev_mouse_btn = mbtn; sleep_ms(16);
    }
}

void desk_remove_icon_by_path(const char* path) { (void)path; }
void desk_add_file_icon(const char* path, const char* name) { (void)path; (void)name; }

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

#define START_MENU_W 340
#define MENU_ITEM_H 40

int gui_running = 1;
static int g_st = 1;
static int m_idx = 0;
static int prev_mouse_btn = 0;

static char search_buf[64];
static int search_len = 0;
static int search_results[16];
static int search_result_count = 0;
static int search_sel = 0;
static int search_open = 0;

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
static const char* apps_menu[] = { "Calculator", "File Explorer", "Text Editor", "Bdrowser", "Calendar", "Process Viewer", "Clock" };
#define APPS_MENU_COUNT 7
static const char* system_menu[] = { "PCI Scanner", "System" };
#define SYSTEM_MENU_COUNT 2
static const char* games_menu[] = { "Piano", "Snake", "Mines", "Tetris", "Pairs" };
#define GAMES_MENU_COUNT 5
static const char* demo_menu[] = { "Teacup" };
#define DEMO_MENU_COUNT 1
static const char* accessibility_menu[] = { "On-Screen Keyboard" };
#define ACCESSIBILITY_MENU_COUNT 1
static const char* graphics_menu[] = { "Image Viewer" };
#define GRAPHICS_MENU_COUNT 1

static const char* all_items[] = { "Calculator", "File Explorer", "PCI Scanner", "System App", "Text Editor", "Teacup", "Terminal", "Process Viewer", "Bdrowser", "Calendar", "Piano", "Snake", "Mines", "Clock", "Tetris", "Pairs", "On-Screen Keyboard", "Image Viewer", 0 };

void draw_premium_wallpaper() {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    if (fw == 0 || fh == 0) return;
    gfx_fill_rect(0, 0, fw, fh, 0x003366);
}

void draw_taskbar() {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    if (fw == 0 || fh == 0) return;
    int taskbar_h = 36, taskbar_y = fh - taskbar_h;
    personalization_t* p = get_personalization();
    uint32_t bg_clr = (p->theme == 0) ? 0x0B0C10 : 0xE8E8E8;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x202124;
    uint32_t accent = get_accent_color();
    int mx = mouse_get_x(), my = mouse_get_y();
    int margin = 4, gap = 2;

    gfx_fill_rect(0, taskbar_y, fw, taskbar_h, bg_clr);
    gfx_draw_hline(0, taskbar_y, fw, (p->theme == 0) ? 0x262830 : 0xDADCE0);

    int btn_w = 40;
    int start_x = margin;
    int start_hover = point_in_rect(mx, my, start_x, taskbar_y, btn_w, taskbar_h);
    if (start_hover) gfx_fill_rect(start_x + 2, taskbar_y + 4, btn_w - 4, taskbar_h - 8, (p->theme == 0) ? 0x262830 : 0xDADCE0);
    draw_start_icon(start_x, taskbar_y, btn_w, taskbar_h);

    int search_x = start_x + btn_w + gap;
    int search_hover = point_in_rect(mx, my, search_x, taskbar_y, btn_w, taskbar_h);
    if (search_hover || search_open) gfx_fill_rect(search_x + 2, taskbar_y + 4, btn_w - 4, taskbar_h - 8, (p->theme == 0) ? 0x262830 : 0xDADCE0);
    draw_search_icon(search_x, taskbar_y, btn_w, taskbar_h);

    int spacer = 8;
    int tab_start = search_x + btn_w + spacer;
    int tab_area_w = fw - tab_start - 150;
    if (tab_area_w < 100) tab_area_w = 100;

    int open_windows = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (wm_get_window_by_index(i)) open_windows++;
    }

    int tab_w = 140;
    if (open_windows > 0) {
        int max_tab_w = (tab_area_w - (open_windows - 1) * gap) / open_windows;
        if (max_tab_w < 80) max_tab_w = 80;
        tab_w = max_tab_w;
    }

    int wx = tab_start;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t* win = wm_get_window_by_index(i);
        if (!win) continue;
        int is_focused = (win->flags & WM_FLAG_FOCUSED) != 0;
        int tab_h = taskbar_h - 8;
        int tab_hover = point_in_rect(mx, my, wx, taskbar_y + 4, tab_w, tab_h);
        uint32_t tab_bg = is_focused ? gfx_darken(bg_clr, 15) : (tab_hover ? gfx_lighten(bg_clr, 10) : bg_clr);
        gfx_fill_rect(wx, taskbar_y + 4, tab_w, tab_h, tab_bg);
        char short_title[16];
        int j = 0;
        while (win->title[j] && j < 14) { short_title[j] = win->title[j]; j++; }
        short_title[j] = 0;
        gfx_draw_string_transparent(wx + 8, taskbar_y + 12, short_title, text_clr);
        wx += tab_w + gap;
    }

    int tray_w = 140, tray_x = fw - tray_w - margin;
    gfx_fill_rect(tray_x, taskbar_y, tray_w, taskbar_h, gfx_darken(bg_clr, 5));

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
    gfx_draw_string_transparent(tray_x + tray_w - clock_w - 10, taskbar_y + 10, t_str, text_clr);
}

static void get_menu_geometry(int* mx, int* my, int* mw, int* mh, int* ic) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    (void)fw;
    *mw = 380; *mx = 8;
    int header_h = 68, footer_h = 8;
    if (g_st == 2) *ic = MAIN_MENU_COUNT;
    else if (g_st == 3) *ic = APPS_MENU_COUNT;
    else if (g_st == 5) *ic = SYSTEM_MENU_COUNT;
    else if (g_st == 6) *ic = DEMO_MENU_COUNT;
    else if (g_st == 7) *ic = GAMES_MENU_COUNT;
    else if (g_st == 9) *ic = ACCESSIBILITY_MENU_COUNT;
    else if (g_st == 11) *ic = GRAPHICS_MENU_COUNT;
    else *ic = 0;

    int content_h = *ic * MENU_ITEM_H;
    *mh = content_h + header_h + footer_h;
    if (*mh < 300) *mh = 300;
    *my = fh - 40 - *mh - 8;
}

static void render_menu() {
    int mx, my, mw, mh, ic; get_menu_geometry(&mx, &my, &mw, &mh, &ic);
    personalization_t* p = get_personalization();
    uint32_t bg_main = (p->theme == 0) ? 0x0D0E12 : 0xF4F6F8;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x1F2937;
    uint32_t head_bg  = (p->theme == 0) ? 0x15171D : 0xFFFFFF;
    uint32_t sel_bg   = (p->theme == 0) ? 0x262830 : 0xE5E7EB;
    uint32_t muted    = (p->theme == 0) ? 0x6D7079 : 0x9CA3AF;
    uint32_t border   = (p->theme == 0) ? 0x262830 : 0xDADCE0;
    uint32_t accent   = get_accent_color();

    gfx_draw_shadow(mx, my, mw, mh, 18);
    gfx_fill_rect(mx, my, mw, mh, bg_main);
    gfx_draw_rect_outline(mx, my, mw, mh, 1, border);

    // Header
    gfx_fill_rect(mx + 1, my + 1, mw - 2, 58, head_bg);
    gfx_draw_hline(mx + 1, my + 59, mw - 2, border);

    // Logo / back indicator in header
    if (g_st == 2) {
        // Draw BEDI start icon
        draw_start_icon(mx + 8, my + 10, 32, 32);
        gfx_draw_string_transparent(mx + 48, my + 22, "BEDI OS", text_clr);
    } else {
        // Back arrow badge
        gfx_fill_rect(mx + 8, my + 18, 20, 20, (p->theme == 0) ? 0x262830 : 0xE5E7EB);
        gfx_draw_string_transparent(mx + 11, my + 22, "<", muted);
        const char* sub = (g_st == 3) ? "Applications" : (g_st == 5) ? "System Tools" : (g_st == 7) ? "Games" : (g_st == 9) ? "Accessibility" : (g_st == 11) ? "Graphics" : "Demo";
        gfx_draw_string_transparent(mx + 36, my + 22, sub, text_clr);
    }

    const char** items = (g_st == 2) ? main_menu : (g_st == 3) ? apps_menu : (g_st == 5) ? system_menu : (g_st == 7) ? games_menu : (g_st == 9) ? accessibility_menu : (g_st == 11) ? graphics_menu : demo_menu;
    int has_arrow_at = (g_st == 2) ? 6 : -1;

    for (int i = 0; i < ic; i++) {
        int iy = my + 68 + i * MENU_ITEM_H;
        int is_sel = (m_idx == i);
        // Selection highlight
        if (is_sel) {
            gfx_fill_rect(mx + 4, iy + 1, mw - 8, MENU_ITEM_H - 2, sel_bg);
            gfx_draw_rect_outline(mx + 4, iy + 1, mw - 8, MENU_ITEM_H - 2, 1, muted);
        }
        // Icon (24x24 centred in 40px row)
        draw_app_icon(items[i], mx + 14, iy + 8);
        // Label
        gfx_draw_string_transparent(mx + 50, iy + 13, items[i], is_sel ? text_clr : muted);
        // Arrow badge for submenu rows
        if (i < has_arrow_at) {
            gfx_draw_string_transparent(mx + mw - 24, iy + 13, ">", muted);
        }
        // Thin separator
        if (i < ic - 1) gfx_draw_hline(mx + 50, iy + MENU_ITEM_H - 1, mw - 60, border);
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
    if (global_idx == 0) calculator();
    else if (global_idx == 1) file_explorer();
    else if (global_idx == 2) pci_scanner_app();
    else if (global_idx == 3) system_app();
    else if (global_idx == 4) text_editor();
    else if (global_idx == 5) cube_3d_app();
    else if (global_idx == 6) terminal_app();
    else if (global_idx == 7) process_viewer_app();
    else if (global_idx == 8) bdrowser();
    else if (global_idx == 9) calendar_app();
    else if (global_idx == 10) piano_app();
    else if (global_idx == 11) snake_app();
    else if (global_idx == 12) mines_app();
    else if (global_idx == 13) clock_app();
    else if (global_idx == 14) tetris_app();
    else if (global_idx == 15) pairs_app();
    else if (global_idx == 16) osk_app();
    else if (global_idx == 17) imgview_app();
}

static void handle_menu_click(int cx, int cy) {
    int mx, my, mw, mh, ic; get_menu_geometry(&mx, &my, &mw, &mh, &ic);
    if (!point_in_rect(cx, cy, mx, my, mw, mh)) { g_st = 1; return; }

    // Back button click (header area on sub-menus)
    if (g_st > 2 && cy < my + 68) { g_st = 2; m_idx = 0; return; }

    int rel_y = cy - (my + 68);
    if (rel_y < 0) return;
    int idx = rel_y / MENU_ITEM_H;

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
        else if (key == '\n') handle_menu_click(mx + 8, my + 68 + m_idx * MENU_ITEM_H + (MENU_ITEM_H / 2));
    }
}

void gui_toggle_start_menu(void) { search_open = 0; if (g_st >= 2) g_st = 1; else g_st = 2; m_idx = 0; }
void gui_open_search(void) { g_st = 1; search_open = 1; search_len = 0; search_buf[0] = 0; search_sel = 0; update_search_results(); }
void gui_toggle_search(void) { if (search_open) search_open = 0; else gui_open_search(); }
int gui_is_menu_open(void) { return g_st >= 2; }
int gui_is_search_open(void) { return search_open; }

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
        int clicked = (mbtn & 1) && !(prev_mouse_btn & 1), taskbar_y = fh - 40, click_handled = 0;
        if (clicked) {
            if (point_in_rect(cx, cy, 4, taskbar_y, 40, 36)) { search_open = 0; if (g_st >= 2) g_st = 1; else g_st = 2; m_idx = 0; click_handled = 1; }
            if (!click_handled && point_in_rect(cx, cy, 46, taskbar_y, 40, 36)) { g_st = 1; gui_toggle_search(); click_handled = 1; }
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
                    int ry = my + 80; // Input height is 70 + 10 margin
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
            if (!click_handled && cy >= taskbar_y && cx >= 92 && cx < (int)fw - 148) {
                int tab_start = 92;
                int tab_area_w = fw - tab_start - 150;
                if (tab_area_w < 100) tab_area_w = 100;
                int open_count = 0;
                for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                    if (wm_get_window_by_index(i)) open_count++;
                }
                int gap = 2;
                int tab_w = 140;
                if (open_count > 0) {
                    int max_tab = (tab_area_w - (open_count - 1) * gap) / open_count;
                    if (max_tab < 80) max_tab = 80;
                    tab_w = max_tab;
                }
                int wx = tab_start;
                for (int i = 0; i < WM_MAX_WINDOWS; i++) {
                    wm_window_t* win = wm_get_window_by_index(i);
                    if (!win) continue;
                    if (point_in_rect(cx, cy, wx, taskbar_y, tab_w, 36)) { wm_bring_to_front(win->id); break; }
                    wx += tab_w + gap;
                }
                click_handled = 1;
            }
            if (!click_handled) { g_st = 1; search_open = 0; }
        }
        if (g_st >= 2) {
            int mmx, mmy, mmw, mmh, ic; get_menu_geometry(&mmx, &mmy, &mmw, &mmh, &ic);
            if(point_in_rect(cx, cy, mmx, mmy, mmw, mmh)) {
                int ry = cy - (mmy + 58); if(ry >= 0) { int i = ry / MENU_ITEM_H; if(i < ic) m_idx = i; }
            }
        }
        draw_premium_wallpaper(); wm_tick(); draw_taskbar();
        if (g_st >= 2) render_menu(); if (search_open) render_search_panel();
        mouse_draw_cursor(); swap_buffers(); prev_mouse_btn = mbtn; sleep_ms(16);
    }
}

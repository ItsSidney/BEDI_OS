#include "../../include/gui.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/commands.h"
#include "../../include/calculator.h"
#include "../../include/editor.h"
#include "../../include/stories.h"
#include "../../include/guessing_game.h"
#include "../../include/rtc.h"
#include "../../include/pcspeaker.h"

extern void doc_viewer();

// Theme Colors
#define CLR_TB     ((VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE)
#define CLR_BTN    ((VGA_COLOR_DARK_GREY << 4) | VGA_COLOR_WHITE)
#define CLR_BTN_ON ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)
#define CLR_MN_BG  ((VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK)
#define CLR_MN_SEL ((VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE)

static int g_st = 1; 
static int m_idx = 0;
static int needs_redraw = 1;
static int h_off = 0;
static int m_off = 0;

static void draw_b(int x, int y, int w, int h, int c) {
    draw_box_vga(x, y, w, h, (c >> 4) & 0x0F);
}

static void get_adj_time(time_t* t) {
    get_time(t);
    t->hour = (t->hour + h_off) % 24;
    t->minute = (t->minute + m_off) % 60;
}

static void show_sound_settings() {
    while (1) {
        restore_background();
        draw_box_vga(20, 8, 40, 10, VGA_COLOR_BLACK);
        draw_box_vga(19, 7, 40, 10, VGA_COLOR_LIGHT_GREY);
        draw_box_vga(19, 7, 40, 1, VGA_COLOR_BLUE);
        set_cursor(21, 7); print_string_color(" Premium Sound Settings ", (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE);
        set_cursor(21, 9); print_string_color("Current Volume: ", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK);
        int vol = get_volume();
        char vs[8]; vs[0]=(vol/100)+'0'; vs[1]=((vol%100)/10)+'0'; vs[2]=(vol%10)+'0'; vs[3]='%'; vs[4]=0;
        print_string_color(vs, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLUE);
        set_cursor(21, 11); print_string_color("Current Pitch:  ", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK);
        uint32_t f = get_freq();
        char fs[10]; fs[0]=(f/1000)+'0'; fs[1]=((f%1000)/100)+'0'; fs[2]=((f%100)/10)+'0'; fs[3]=(f%10)+'0'; fs[4]='H'; fs[5]='z'; fs[6]=0;
        print_string_color(fs, (VGA_COLOR_WHITE << 4) | VGA_COLOR_GREEN);
        set_cursor(21, 13); 
        if (get_mute()) print_string_color("MUTE STATUS:    [ MUTED ]", (VGA_COLOR_WHITE << 4) | VGA_COLOR_RED);
        else print_string_color("MUTE STATUS:    [ ENABLED ]", (VGA_COLOR_WHITE << 4) | VGA_COLOR_GREEN);
        set_cursor(21, 15); print_string_color("CONTROLS:", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK);
        set_cursor(21, 16); print_string_color("UP/DN: Pitch | L/R: Volume | M: Mute", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_DARK_GREY);
        swap_buffers();
        char k = get_key();
        if (k == KEY_ESC) break;
        else if (k == KEY_LEFT) { int v = get_volume(); if (v > 0) set_volume(v-10); beep(); }
        else if (k == KEY_RIGHT) { int v = get_volume(); if (v < 100) set_volume(v+10); beep(); }
        else if (k == KEY_UP) { set_freq(get_freq() + 100); beep(); }
        else if (k == KEY_DOWN) { set_freq(get_freq() - 100); beep(); }
        else if (k == 'm' || k == 'M') { set_mute(!get_mute()); if(!get_mute()) beep(); }
    }
}

static void show_time_settings() {
    int last_h = -1, last_m = -1;
    while (1) {
        time_t t; get_adj_time(&t);
        if (t.hour != last_h || t.minute != last_m) {
            restore_background();
            draw_box_vga(20, 8, 40, 10, VGA_COLOR_BLACK);
            draw_box_vga(19, 7, 40, 10, VGA_COLOR_LIGHT_GREY);
            draw_box_vga(19, 7, 40, 1, VGA_COLOR_BLUE);
            set_cursor(21, 7); print_string_color(" System Time Settings ", (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE);
            set_cursor(21, 10); print_string_color("CURRENT SYSTEM TIME:", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_DARK_GREY);
            char ts[10]; ts[0]=(t.hour/10)+'0'; ts[1]=(t.hour%10)+'0'; ts[2]=':'; ts[3]=(t.minute/10)+'0'; ts[4]=(t.minute%10)+'0'; ts[5]=0;
            set_cursor(35, 11); print_string_color(ts, (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE);
            set_cursor(21, 14); print_string_color("UP/DN: Hours | L/R: Minutes", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK);
            set_cursor(21, 15); print_string_color("Press ESC to Save & Exit", (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLUE);
            swap_buffers();
            last_h = t.hour; last_m = t.minute;
        }
        char k = get_key();
        if (k == 0) { for (volatile int i=0; i<10000; i++); continue; }
        if (k == KEY_ESC) { beep(); break; }
        else if (k == KEY_UP) { h_off++; last_h = -1; }
        else if (k == KEY_DOWN) { h_off--; last_h = -1; }
        else if (k == KEY_LEFT) { m_off--; last_h = -1; }
        else if (k == KEY_RIGHT) { m_off++; last_h = -1; }
    }
}

static void show_about_scrolling() {
    int scroll = 0, last_scroll = -1;
    const char* about_text[] = {
        "BEDI OPERATING SYSTEM", "====================", "Version: 5.2.0 Elite", "Arch: x86_64 UEFI", "",
        "Developed by Sidney.", "", "SYSTEM SPECS:", "Target: HP Elite x2 G2", "Display: 1024x768 32bpp",
        "Mode: Professional GUI", "", "CONTROLS:", "- Arrows: Navigation", "- Enter: Launch", "- ESC: Back/Exit",
        "", "(c) 2026 BEDI SOFTWARE"
    };
    int total_lines = 19;
    while (1) {
        if (scroll != last_scroll) {
            restore_background();
            draw_box_vga(16, 6, 48, 14, VGA_COLOR_BLACK);
            draw_box_vga(15, 5, 48, 14, VGA_COLOR_LIGHT_GREY);
            draw_box_vga(15, 5, 48, 1, VGA_COLOR_BLUE);
            set_cursor(17, 5); print_string_color("About BEDI OS", (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE);
            for (int i = 0; i < 11; i++) {
                if (scroll+i < total_lines) {
                    set_cursor(17, 7+i);
                    print_string_color(about_text[scroll+i], (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK);
                }
            }
            swap_buffers();
            last_scroll = scroll;
        }
        char k = get_key();
        if (k == 0) { for (volatile int i=0; i<10000; i++); continue; }
        if (k == KEY_ESC) break;
        else if (k == KEY_UP && scroll > 0) scroll--;
        else if (k == KEY_DOWN && scroll < total_lines-11) scroll++;
    }
}

static void render_desktop_base() {
    draw_gradient_background();
    draw_b(0, 24, 80, 1, CLR_TB);
    cache_background();
}

static void render_all() {
    restore_background();
    int sc = (g_st == 2) ? CLR_BTN_ON : CLR_BTN;
    draw_b(0, 24, 14, 1, sc);
    set_cursor(2, 24); print_string_color(" [ START ] ", sc);
    time_t t; get_adj_time(&t);
    char time_str[9];
    time_str[0]=(t.hour/10)+'0'; time_str[1]=(t.hour%10)+'0'; time_str[2]=':';
    time_str[3]=(t.minute/10)+'0'; time_str[4]=(t.minute%10)+'0'; time_str[5]=':';
    time_str[6]=(t.second/10)+'0'; time_str[7]=(t.second%10)+'0'; time_str[8]=0;
    set_cursor(70, 24); print_string_color(time_str, CLR_TB);

    if (g_st == 2) { 
        draw_box_vga(1, 14, 26, 10, VGA_COLOR_BLACK);
        draw_b(0, 13, 26, 11, CLR_MN_BG);
        const char* its[] = {" Apps >", " Games >", " Settings >", " About", " Exit GUI"};
        for (int i = 0; i < 5; i++) {
            int c = (m_idx == i) ? CLR_MN_SEL : CLR_MN_BG;
            draw_b(0, 14+i, 26, 1, c);
            set_cursor(2, 14+i); print_string_color(its[i], c);
        }
    } else if (g_st == 3) { 
        draw_box_vga(27, 13, 22, 4, VGA_COLOR_BLACK);
        draw_b(26, 12, 22, 5, CLR_MN_BG);
        const char* its[] = {" Calculator", " Text Editor", " Document Viewer", " Stories"};
        for (int i = 0; i < 4; i++) {
            int c = (m_idx == i) ? CLR_MN_SEL : CLR_MN_BG;
            draw_b(26, 13+i, 22, 1, c);
            set_cursor(28, 13+i); print_string_color(its[i], c);
        }
    } else if (g_st == 4) { 
        draw_box_vga(27, 15, 22, 1, VGA_COLOR_BLACK);
        draw_b(26, 14, 22, 2, CLR_MN_BG);
        const char* its[] = {" Guessing Game"};
        for (int i = 0; i < 1; i++) {
            int c = (m_idx == i) ? CLR_MN_SEL : CLR_MN_BG;
            draw_b(26, 15+i, 22, 1, c);
            set_cursor(28, 15+i); print_string_color(its[i], c);
        }
    } else if (g_st == 5) { 
        draw_box_vga(27, 16, 22, 2, VGA_COLOR_BLACK);
        draw_b(26, 15, 22, 3, CLR_MN_BG);
        const char* its[] = {" Time Setting", " Sound Settings"};
        for (int i = 0; i < 2; i++) {
            int c = (m_idx == i) ? CLR_MN_SEL : CLR_MN_BG;
            draw_b(26, 16+i, 22, 1, c);
            set_cursor(28, 16+i); print_string_color(its[i], c);
        }
    }
    swap_buffers();
}

void start_gui() {
    g_st = 1; m_idx = 0; needs_redraw = 1;
    clear_screen(); render_desktop_base();
    int last_s = -1;
    while (1) {
        time_t t; get_adj_time(&t);
        if (t.second != last_s) { last_s = t.second; needs_redraw = 1; }
        char key = get_key();
        if (key != 0) {
            needs_redraw = 1;
            if (key == '\n') beep();
            if (key == KEY_ESC) { if (g_st > 2) g_st = 2; else g_st = 1; m_idx = 0; }
            else if (key == KEY_UP) { if (m_idx > 0) m_idx--; }
            else if (key == KEY_DOWN) { 
                int max = (g_st == 2) ? 4 : (g_st == 3) ? 3 : (g_st == 4) ? 0 : 1;
                if (m_idx < max) m_idx++;
            }
            else if (key == '\n') {
                if (g_st == 1) { g_st = 2; m_idx = 0; }
                else if (g_st == 2) {
                    if (m_idx == 0) { g_st = 3; m_idx = 0; }
                    else if (m_idx == 1) { g_st = 4; m_idx = 0; }
                    else if (m_idx == 2) { g_st = 5; m_idx = 0; }
                    else if (m_idx == 3) { g_st = 1; show_about_scrolling(); }
                    else if (m_idx == 4) { clear_screen(); return; } 
                    clear_screen(); render_desktop_base();
                } else if (g_st == 3) { 
                    g_st = 1; 
                    if (m_idx == 0) calculator(); 
                    else if (m_idx == 1) editor(); // WE REMOVED CLEAR_SCREEN FROM HERE
                    else if (m_idx == 2) doc_viewer();
                    else { restore_background(); stories_app(); }
                    clear_screen(); render_desktop_base();
                } else if (g_st == 4) { 
                    g_st = 1; guessing_game();
                    clear_screen(); render_desktop_base();
                } else if (g_st == 5) { 
                    g_st = 1;
                    if (m_idx == 0) show_time_settings();
                    else show_sound_settings();
                    clear_screen(); render_desktop_base();
                }
            }
        }
        if (needs_redraw) { render_all(); needs_redraw = 0; }
        else { for (volatile int i = 0; i < 50000; i++); }
    }
}

#include "gui/app_icons.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"

// ── Calculator: chrome body, color-coded key grid ──────────────────────────
static void draw_pro_calc(int x, int y) {
    gfx_fill_rect(x+1, y, 22, 24, 0x374151);
    gfx_draw_rect_outline(x+1, y, 22, 24, 1, 0x111827);
    gfx_fill_rect(x+3, y+1, 18, 7, 0x1E293B);
    gfx_fill_rect(x+4, y+2, 16, 5, 0x0F172A);
    gfx_draw_string_transparent(x+5, y+2, "0.", 0x34D399);
    uint32_t kc[] = {0xF87171,0xFBBF24,0x60A5FA,0x34D399};
    for(int r=0;r<3;r++) for(int c=0;c<3;c++) {
        uint32_t col = (r==0&&c==2)?kc[0]:(r==1&&c==2)?kc[1]:(r==2&&c==2)?kc[2]:0x9CA3AF;
        gfx_fill_rect(x+3+c*6, y+10+r*4, 5, 3, col);
    }
}

// ── Folder / File Explorer: layered tab folder ─────────────────────────────
static void draw_pro_folder(int x, int y) {
    gfx_fill_rect(x+1, y+5, 11, 3, 0xD97706);
    gfx_fill_rect(x, y+7, 24, 15, 0xF59E0B);
    gfx_draw_rect_outline(x, y+7, 24, 15, 1, 0x92400E);
    gfx_fill_rect(x+1, y+9, 22, 2, 0xFCD34D);
    gfx_fill_rect(x+3, y+12, 18, 2, 0xFDE68A);
    gfx_fill_rect(x+3, y+15, 14, 2, 0xFDE68A);
}

// ── Terminal: dark console, traffic-light dots, green prompt ───────────────
static void draw_pro_terminal(int x, int y) {
    gfx_fill_rect(x, y+2, 24, 20, 0x0F172A);
    gfx_draw_rect_outline(x, y+2, 24, 20, 1, 0x334155);
    gfx_fill_rect(x, y+2, 24, 5, 0x1E293B);
    gfx_fill_circle(x+4,  y+4, 2, 0xEF4444);
    gfx_fill_circle(x+9,  y+4, 2, 0xFBBF24);
    gfx_fill_circle(x+14, y+4, 2, 0x10B981);
    gfx_draw_string_transparent(x+2, y+9,  ">_", 0x10B981);
    gfx_draw_hline(x+2, y+16, 10, 0x334155);
    gfx_draw_hline(x+2, y+18, 6,  0x334155);
}

// ── Calendar: flip-calendar with binder rings, bold red header ────────────
static void draw_pro_calendar(int x, int y) {
    gfx_fill_rect(x+2, y+2, 20, 20, 0xFFFFFF);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0x9CA3AF);
    gfx_fill_rect(x+2, y+2, 20, 7, 0xEF4444);
    gfx_draw_hline(x+2, y+9, 20, 0xDC2626);
    // Rings
    gfx_fill_rect(x+6, y,  3, 4, 0x4B5563);
    gfx_fill_rect(x+15, y, 3, 4, 0x4B5563);
    gfx_fill_rect(x+7, y+1, 1, 2, 0x9CA3AF);
    gfx_fill_rect(x+16, y+1,1, 2, 0x9CA3AF);
    // Day number "23"
    gfx_draw_string_transparent(x+6, y+11, "23", 0x111827);
    gfx_draw_hline(x+4, y+19, 16, 0xE5E7EB);
}

// ── Bdrowser: globe with latitude / meridian lines ─────────────────────────
static void draw_pro_bdrowser(int x, int y) {
    int cx = x+12, cy = y+12;
    gfx_fill_circle(cx, cy, 10, 0x2563EB);
    gfx_draw_circle(cx, cy, 10, 0x1D4ED8);
    gfx_draw_hline(x+2, cy,    20, 0x93C5FD);
    gfx_draw_hline(x+4, cy-4,  16, 0x93C5FD);
    gfx_draw_hline(x+4, cy+4,  16, 0x93C5FD);
    gfx_draw_vline(cx,  y+2,   20, 0x93C5FD);
    gfx_fill_circle(cx-4, cy-4, 2, 0xFFFFFF);
}

// ── Process Viewer: EKG monitor ────────────────────────────────────────────
static void draw_pro_process(int x, int y) {
    gfx_fill_rect(x+1, y+1, 22, 22, 0x0F172A);
    gfx_draw_rect_outline(x+1, y+1, 22, 22, 1, 0x334155);
    gfx_draw_hline(x+2, y+8,  20, 0x1E293B);
    gfx_draw_hline(x+2, y+14, 20, 0x1E293B);
    // EKG line
    gfx_draw_line(x+2,  y+12, x+7,  y+12, 0x10B981);
    gfx_draw_line(x+7,  y+12, x+10, y+4,  0x10B981);
    gfx_draw_line(x+10, y+4,  x+13, y+20, 0x10B981);
    gfx_draw_line(x+13, y+20, x+16, y+12, 0x10B981);
    gfx_draw_line(x+16, y+12, x+22, y+12, 0x10B981);
}

// ── Text Editor: document with dog-eared corner ────────────────────────────
static void draw_pro_text(int x, int y) {
    gfx_fill_rect(x+3, y+1, 16, 22, 0xF8FAFC);
    gfx_draw_rect_outline(x+3, y+1, 16, 22, 1, 0xCBD5E1);
    // Dog-ear
    gfx_fill_rect(x+14, y+1, 5, 5, 0xE2E8F0);
    gfx_draw_line(x+14, y+1, x+14, y+6, 0xCBD5E1);
    gfx_draw_line(x+14, y+6, x+19, y+6, 0xCBD5E1);
    // Text lines
    gfx_draw_hline(x+5, y+8,  11, 0x3B82F6);
    gfx_draw_hline(x+5, y+11, 11, 0x94A3B8);
    gfx_draw_hline(x+5, y+14, 11, 0x94A3B8);
    gfx_draw_hline(x+5, y+17, 7,  0x94A3B8);
}

// ── PCI Scanner: circuit-board chip ───────────────────────────────────────
static void draw_pro_pci(int x, int y) {
    gfx_fill_rect(x+4, y+4, 16, 16, 0x166534);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x14532D);
    gfx_fill_rect(x+7, y+7, 10, 10, 0x1A1A2E);
    gfx_draw_rect_outline(x+7, y+7, 10, 10, 1, 0x16213E);
    // Pins left/right
    for(int i=0;i<3;i++) {
        gfx_fill_rect(x+1, y+6+i*4, 3, 2, 0xFBBF24);
        gfx_fill_rect(x+20, y+6+i*4, 3, 2, 0xFBBF24);
    }
    gfx_fill_circle(x+12, y+12, 3, 0x60A5FA);
}

// ── Teacup 3D: stylised teacup icon ────────────────────────────────────────
static void draw_pro_cube(int x, int y) {
    // Cup body (trapezoid)
    gfx_draw_line(x+4,  y+12, x+6,  y+4,  0x8AB4F8);
    gfx_draw_line(x+6,  y+4,  x+18, y+4,  0x8AB4F8);
    gfx_draw_line(x+18, y+4,  x+20, y+12, 0x8AB4F8);
    gfx_draw_line(x+20, y+12, x+4,  y+12, 0x8AB4F8);
    // Base
    gfx_draw_line(x+6,  y+16, x+18, y+16, 0x8AB4F8);
    gfx_draw_line(x+4,  y+12, x+6,  y+16, 0x8AB4F8);
    gfx_draw_line(x+20, y+12, x+18, y+16, 0x8AB4F8);
    // Handle (right side)
    gfx_draw_line(x+20, y+6,  x+23, y+8,  0x8AB4F8);
    gfx_draw_line(x+23, y+8,  x+23, y+12, 0x8AB4F8);
    gfx_draw_line(x+23, y+12, x+20, y+12, 0x8AB4F8);
    // Fill
    gfx_fill_rect(x+7,  y+5,  10, 7,  0x60A5FA);
    gfx_fill_rect(x+7,  y+13, 10, 2,  0x2563EB);
    // Steam
    gfx_draw_line(x+10, y+2,  x+10, y+0,  0x93C5FD);
    gfx_draw_line(x+14, y+1,  x+14, y-1,  0x93C5FD);
}

// ── System Info: stylized "i" pill badge ───────────────────────────────────
static void draw_pro_system(int x, int y) {
    gfx_fill_rect(x+4, y+2, 16, 20, 0x7C3AED);
    gfx_draw_rect_outline(x+4, y+2, 16, 20, 1, 0x5B21B6);
    gfx_fill_rect(x+10, y+5, 4, 4, 0xFFFFFF);
    gfx_fill_rect(x+10, y+11, 4, 9, 0xFFFFFF);
    gfx_draw_hline(x+7, y+11, 10, 0xA78BFA);
    gfx_draw_hline(x+7, y+20, 10, 0xA78BFA);
}

// ── Clock: simple clock face ──────────────────────────────────────────────
static void draw_pro_clock(int x, int y) {
    gfx_draw_circle(x+12, y+12, 10, 0x34D399);
    gfx_draw_circle(x+12, y+12, 8, 0x34D399);
    gfx_fill_circle(x+12, y+12, 7, 0x0D0D1A);
    // Hour hand (pointing roughly right-up)
    gfx_draw_line(x+12, y+12, x+17, y+8,  0x34D399);
    // Minute hand (pointing right)
    gfx_draw_line(x+12, y+12, x+19, y+12, 0x34D399);
    // Center dot
    gfx_fill_circle(x+12, y+12, 2, 0x34D399);
}

// ── Shutdown: power button ─────────────────────────────────────────────────
static void draw_pro_shutdown(int x, int y) {
    gfx_draw_circle(x+12, y+13, 9, 0xEF4444);
    gfx_draw_vline(x+12, y+4, 9, 0xEF4444);
    gfx_fill_rect(x+11, y+4, 3, 3, 0xEF4444);
}

// ── Games: gamepad ────────────────────────────────────────────────────────
static void draw_pro_games(int x, int y) {
    gfx_fill_rect(x+3,  y+8,  18, 8,  0x60A5FA);
    gfx_draw_rect_outline(x+3, y+8, 18, 8,  1, 0x1D4ED8);
    gfx_fill_rect(x+1,  y+10, 2,  4,  0x60A5FA);
    gfx_fill_rect(x+21, y+10, 2,  4,  0x60A5FA);
    gfx_fill_rect(x+6,  y+10, 4,  2,  0x1E293B);
    gfx_fill_rect(x+7,  y+9,  2,  4,  0x1E293B);
    gfx_fill_circle(x+16, y+10, 2, 0xEF4444);
    gfx_fill_circle(x+19, y+10, 2, 0x34D399);
}

// ── Mines: mine icon ──────────────────────────────────────────────────────
static void draw_pro_mines(int x, int y) {
    gfx_fill_rect(x+4,  y+4,  16, 16, 0x374151);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x1E293B);
    gfx_fill_circle(x+12, y+12, 5, 0x111111);
    gfx_draw_circle(x+12, y+12, 5, 0x444444);
    gfx_draw_line(x+12, y+3,  x+12, y+7,  0x9CA3AF);
    gfx_draw_line(x+12, y+17, x+12, y+21, 0x9CA3AF);
    gfx_draw_line(x+3,  y+12, x+7,  y+12, 0x9CA3AF);
    gfx_draw_line(x+17, y+12, x+21, y+12, 0x9CA3AF);
    gfx_draw_line(x+6,  y+6,  x+9,  y+9,  0x9CA3AF);
    gfx_draw_line(x+18, y+6,  x+15, y+9,  0x9CA3AF);
    gfx_draw_line(x+6,  y+18, x+9,  y+15, 0x9CA3AF);
    gfx_draw_line(x+18, y+18, x+15, y+15, 0x9CA3AF);
    gfx_fill_circle(x+10, y+10, 2, 0x666666);
}

// ── Pairs: two matching cards ─────────────────────────────────────────────
static void draw_pro_pairs(int x, int y) {
    gfx_fill_rect(x+1,  y+3,  10, 18, 0x1E1E3A);
    gfx_draw_rect_outline(x+1, y+3, 10, 18, 1, 0x333355);
    gfx_draw_string_transparent(x+4, y+8, "?", 0x555577);
    gfx_fill_rect(x+13, y+3,  10, 18, 0xF472B6);
    gfx_draw_rect_outline(x+13, y+3, 10, 18, 1, 0xDB2777);
    gfx_draw_string_transparent(x+16, y+8, "A", 0xFFFFFF);
    gfx_draw_line(x+11, y+5,  x+11, y+19, 0x34D399);
    gfx_draw_line(x+11, y+5,  x+13, y+7,  0x34D399);
}

// ── Tetris: tetromino stack ───────────────────────────────────────────────
static void draw_pro_tetris(int x, int y) {
    gfx_fill_rect(x+3, y+2, 18, 20, 0x0D0D1A);
    gfx_draw_rect_outline(x+3, y+2, 18, 20, 1, 0x2A2A4A);
    static const int shape[] = {
        1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,0,0,0,
        0,0,0,0,1,1,0,0,1,1,0,0,0,0,0,0,0,0,
        0,0,0,0,0,1,0,0,1,1,0,0,0,1,0,0,0,0,
    };
    uint32_t cols[] = {0x00F0F0,0xF0F000,0xA000F0,0x00F000,0xF00000,0x0000F0,0xF0A000};
    int ci = 0;
    for (int gy = 0; gy < 4; gy++)
        for (int gx = 0; gx < 6; gx++) {
            int idx = gy * 6 + gx;
            if (shape[idx]) {
                gfx_fill_rect(x+4+gx*3, y+3+gy*4, 2, 3, cols[ci % 7]);
                ci++;
            }
        }
}

// ── Snake: green serpent ──────────────────────────────────────────────────
static void draw_pro_snake(int x, int y) {
    gfx_fill_rect(x+4,  y+2,  16, 20, 0x065F46);
    gfx_draw_rect_outline(x+4, y+2, 16, 20, 1, 0x047857);
    gfx_fill_rect(x+6,  y+4,  4,  2,  0x34D399);
    gfx_fill_rect(x+6,  y+7,  4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+4,  4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+7,  4,  5,  0x34D399);
    gfx_fill_rect(x+6,  y+13, 4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+13, 4,  2,  0x34D399);
    gfx_fill_rect(x+6,  y+16, 8,  2,  0x34D399);
    gfx_fill_rect(x+14, y+16, 4,  2,  0x34D399);
    gfx_fill_rect(x+7,  y+5,  1,  1,  0xFFFFFF);
    gfx_fill_circle(x+11, y+5, 1, 0xFFFFFF);
    gfx_fill_circle(x+11, y+5, 1, 0x000000);
    gfx_fill_rect(x+15, y+9,  3,  3,  0xEF4444);
}

// ── Piano: keyboard keys ──────────────────────────────────────────────────
static void draw_pro_piano(int x, int y) {
    for (int i = 0; i < 7; i++) {
        gfx_fill_rect(x+1+i*3, y+5, 2, 18, 0xF8FAFC);
        gfx_draw_rect_outline(x+1+i*3, y+5, 2, 18, 1, 0x94A3B8);
    }
    int bk[] = {4, 7, 13, 16, 19};
    for (int i = 0; i < 5; i++) {
        gfx_fill_rect(x+bk[i], y+4, 2, 11, 0x1E293B);
        gfx_draw_rect_outline(x+bk[i], y+4, 2, 11, 1, 0x0F172A);
    }
    gfx_draw_string_transparent(x+2, y+1, "♪", 0x8AB4F8);
}

// ── Accessibility: person icon ──────────────────────────────────────────
static void draw_pro_accessibility(int x, int y) {
    gfx_fill_circle(x+12, y+6,  3, 0xE4E6EA);
    gfx_fill_rect(x+8,  y+11, 8, 7, 0xE4E6EA);
    gfx_fill_rect(x+11, y+11, 2, 7, 0x4D5059);
    gfx_draw_line(x+5,  y+13, x+19, y+13, 0xE4E6EA);
    gfx_draw_line(x+5,  y+15, x+19, y+15, 0xE4E6EA);
}

// ── On-Screen Keyboard icon ──────────────────────────────────────────────
static void draw_pro_osk(int x, int y) {
    gfx_fill_rect(x+3, y+6, 18, 12, 0x1D1F26);
    gfx_draw_rect_outline(x+3, y+6, 18, 12, 1, 0x383B44);
    uint32_t kc = 0x4D5059;
    // Row 1
    gfx_fill_rect(x+5, y+8,  2, 2, kc); gfx_fill_rect(x+8, y+8,  2, 2, kc);
    gfx_fill_rect(x+11, y+8, 2, 2, kc); gfx_fill_rect(x+14, y+8, 2, 2, kc);
    gfx_fill_rect(x+17, y+8, 2, 2, kc);
    // Row 2
    gfx_fill_rect(x+5, y+11, 3, 2, kc); gfx_fill_rect(x+9, y+11, 2, 2, kc);
    gfx_fill_rect(x+12, y+11,2, 2, kc); gfx_fill_rect(x+15, y+11,3, 2, kc);
    // Row 3 (space bar)
    gfx_fill_rect(x+7, y+14, 10, 2, kc);
}

// ── Graphics: mountain landscape icon ─────────────────────────────────────
static void draw_pro_graphics(int x, int y) {
    gfx_fill_rect(x+4, y+4, 16, 16, 0x0D0E12);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x383B44);
    gfx_fill_circle(x+16, y+7, 3, 0xFBBF24);
    gfx_fill_rect(x+5, y+17, 14, 3, 0x22D3EE);
    gfx_fill_rect(x+7, y+13, 5, 5, 0x4D5059);
    gfx_fill_rect(x+12, y+14, 4, 4, 0x4D5059);
    gfx_fill_rect(x+16, y+12, 3, 5, 0x4D5059);
}

// ── Image Viewer: picture frame icon ─────────────────────────────────────
static void draw_pro_imgview(int x, int y) {
    gfx_fill_rect(x+3, y+4, 18, 16, 0x1D1F26);
    gfx_draw_rect_outline(x+3, y+4, 18, 16, 1, 0x4D5059);
    gfx_fill_rect(x+5, y+6, 14, 12, 0x0A0D16);
    gfx_fill_circle(x+8, y+9, 2, 0xFBBF24);
    gfx_fill_rect(x+5, y+14, 14, 4, 0x22D3EE);
    gfx_fill_rect(x+12, y+11, 3, 4, 0x4D5059);
}

// ── Dispatch ──────────────────────────────────────────────────────────────
void draw_app_icon(const char* name, int x, int y) {
    if (!name) goto fallback;
    // Match by first 4 chars
    if      (name[0]=='C'&&name[1]=='a'&&name[2]=='l'&&name[3]=='e') draw_pro_calendar(x, y);
    else if (name[0]=='C'&&name[1]=='a'&&name[2]=='l'&&name[3]=='c') draw_pro_calc(x, y);
    else if (name[0]=='F'&&name[1]=='i') draw_pro_folder(x, y);
    else if (name[0]=='T'&&name[1]=='e'&&name[2]=='r') draw_pro_terminal(x, y);
    else if (name[0]=='T'&&name[1]=='e'&&name[2]=='t') draw_pro_tetris(x, y);
    else if (name[0]=='P'&&name[1]=='a') draw_pro_pairs(x, y);    // "Pairs"
    else if (name[0]=='T'&&name[1]=='e'&&name[2]=='a') draw_pro_cube(x, y);    // "Teacup"
    else if (name[0]=='T'&&name[1]=='e'&&name[2]=='x') draw_pro_text(x, y);
    else if (name[0]=='B'&&name[1]=='d') draw_pro_bdrowser(x, y);
    else if (name[0]=='P'&&name[1]=='r'&&name[2]=='o'&&name[3]=='c') draw_pro_process(x, y);
    else if (name[0]=='P'&&name[1]=='C') draw_pro_pci(x, y);
    else if (name[0]=='C'&&name[1]=='u') draw_pro_cube(x, y);
    else if (name[0]=='S'&&name[1]=='y') draw_pro_system(x, y);
    else if (name[0]=='S'&&name[1]=='h') draw_pro_shutdown(x, y);
    else if (name[0]=='A'&&name[1]=='c') draw_pro_accessibility(x, y); // "Accessibility"
    else if (name[0]=='A'&&name[1]=='p') draw_pro_folder(x, y);   // "Applications"
    else if (name[0]=='O'&&name[1]=='n') draw_pro_osk(x, y);      // "On-Screen Keyboard"
    else if (name[0]=='C'&&name[1]=='l') draw_pro_clock(x, y);    // "Clock"
    else if (name[0]=='D'&&name[1]=='e') draw_pro_cube(x, y);     // "Demo"
    else if (name[0]=='G'&&name[1]=='a') draw_pro_games(x, y);   // "Games"
    else if (name[0]=='G'&&name[1]=='r') draw_pro_graphics(x, y); // "Graphics"
    else if (name[0]=='I'&&name[1]=='m') draw_pro_imgview(x, y);  // "Image Viewer"
    else if (name[0]=='P'&&name[1]=='i') draw_pro_piano(x, y);    // "Piano"
    else if (name[0]=='S'&&name[1]=='n') draw_pro_snake(x, y);    // "Snake"
    else if (name[0]=='M'&&name[1]=='i') draw_pro_mines(x, y);    // "Mines"
    else { fallback:
        gfx_fill_rect(x+4, y+4, 16, 16, 0x334155);
        gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x1E293B);
        gfx_fill_rect(x+8, y+8,  8,  8,  0x475569);
    }
}

// ── BEDI OS Start logo: Minimalist geometric glowing star/diamond ───────────
void draw_start_icon(int x, int y, int w, int h) {
    personalization_t* p = get_personalization();
    uint32_t c = get_accent_color();
    uint32_t c_light = gfx_lighten(c, 40);
    uint32_t c_dark = gfx_darken(c, 30);
    int cx = x + w / 2;
    int cy = y + h / 2;

    // Diamond outer frame
    gfx_draw_line(cx, cy - 10, cx + 10, cy, c_dark);
    gfx_draw_line(cx + 10, cy, cx, cy + 10, c_dark);
    gfx_draw_line(cx, cy + 10, cx - 10, cy, c_dark);
    gfx_draw_line(cx - 10, cy, cx, cy - 10, c_dark);

    // Inner glowing star rays
    gfx_draw_line(cx, cy - 6, cx + 2, cy - 2, c_light);
    gfx_draw_line(cx + 2, cy - 2, cx + 6, cy, c_light);
    gfx_draw_line(cx + 6, cy, cx + 2, cy + 2, c_light);
    gfx_draw_line(cx + 2, cy + 2, cx, cy + 6, c_light);
    gfx_draw_line(cx, cy + 6, cx - 2, cy + 2, c_light);
    gfx_draw_line(cx - 2, cy + 2, cx - 6, cy, c_light);
    gfx_draw_line(cx - 6, cy, cx - 2, cy - 2, c_light);
    gfx_draw_line(cx - 2, cy - 2, cx, cy - 6, c_light);

    // Center core
    gfx_fill_circle(cx, cy, 2, c);
    
    (void)p;
}

// ── Search magnifier icon ─────────────────────────────────────────────────
void draw_search_icon(int x, int y, int w, int h) {
    int cx = x + w / 2;
    int cy = y + h / 2;
    personalization_t* p = get_personalization();
    uint32_t c = (p->theme == 0) ? 0xCBD5E1 : 0x475569;
    gfx_draw_circle(cx - 3, cy - 3, 5, c);
    gfx_draw_circle(cx - 3, cy - 3, 4, c);
    gfx_draw_line(cx + 1, cy + 1, cx + 6, cy + 6, c);
    gfx_fill_circle(cx + 6, cy + 6, 1, c);
}

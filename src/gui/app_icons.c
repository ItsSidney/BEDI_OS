#include "gui/app_icons.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"

// ── Calculator: chrome body, color-coded key grid ──────────────────────────
static void draw_pro_calc(int x, int y) {
    gfx_fill_rect(x+2, y+2, 22, 24, 0x1A1D23);
    gfx_fill_rect(x+1, y+1, 22, 24, 0x374151);
    gfx_draw_rect_outline(x+1, y+1, 22, 24, 1, 0x111827);
    gfx_fill_rect(x+3, y+2, 18, 7, 0x0F172A);
    gfx_fill_rect(x+4, y+3, 16, 5, 0x1E293B);
    gfx_draw_string_transparent(x+5, y+3, "0.", 0x34D399);
    gfx_draw_string_transparent(x+13, y+3, "0", 0x34D399);
    uint32_t kc[] = {0xF87171,0xFBBF24,0x60A5FA,0x34D399};
    for(int r=0;r<3;r++) for(int c=0;c<3;c++) {
        uint32_t col = (r==0&&c==2)?kc[0]:(r==1&&c==2)?kc[1]:(r==2&&c==2)?kc[2]:0x9CA3AF;
        gfx_fill_rect(x+3+c*6, y+11+r*4, 5, 3, 0x4B5563);
        gfx_fill_rect(x+3+c*6, y+11+r*4, 5, 2, col);
    }
    gfx_fill_rect(x+21, y+11, 2, 3, 0xF87171);
}

// ── Folder / File Explorer: layered tab folder ─────────────────────────────
static void draw_pro_folder(int x, int y) {
    gfx_fill_rect(x+2, y+4, 22, 18, 0xD97706);
    gfx_draw_rect_outline(x+2, y+4, 22, 18, 1, 0x92400E);
    gfx_fill_rect(x, y+7, 24, 15, 0xF59E0B);
    gfx_draw_rect_outline(x, y+7, 24, 15, 1, 0x92400E);
    gfx_fill_rect(x+1, y+5, 11, 3, 0xFCD34D);
    gfx_draw_rect_outline(x+1, y+5, 11, 3, 1, 0xB45309);
    gfx_fill_rect(x+1, y+8, 22, 2, 0xFCD34D);
    gfx_fill_rect(x+3, y+12, 18, 2, 0xFDE68A);
    gfx_fill_rect(x+3, y+15, 14, 2, 0xFDE68A);
}

// ── Terminal: dark console, traffic-light dots, green prompt ───────────────
static void draw_pro_terminal(int x, int y) {
    gfx_fill_rect(x+2, y+2, 22, 22, 0x334155);
    gfx_draw_rect_outline(x+2, y+2, 22, 22, 1, 0x1E293B);
    gfx_fill_rect(x+4, y+4, 18, 16, 0x0F172A);
    gfx_draw_rect_outline(x+4, y+4, 18, 16, 1, 0x1E293B);
    gfx_fill_rect(x+5, y+5, 16, 14, 0x111827);
    gfx_draw_string_transparent(x+6, y+7, ">", 0x10B981);
    gfx_draw_hline(x+5, y+10, 10, 0x10B981);
    gfx_draw_hline(x+5, y+12, 8, 0x10B981);
    gfx_draw_hline(x+5, y+14, 12, 0x10B981);
    gfx_fill_rect(x+10, y+24, 6, 2, 0x475569);
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
    gfx_fill_rect(x+3, y+3, 18, 18, 0x2563EB);
    gfx_draw_rect_outline(x+3, y+3, 18, 18, 1, 0x1D4ED8);
    gfx_fill_circle(cx, cy, 8, 0x3B82F6);
    gfx_draw_circle(cx, cy, 8, 0x2563EB);
    gfx_draw_hline(x+4, cy, 16, 0x93C5FD);
    gfx_draw_hline(x+6, cy-4, 12, 0x93C5FD);
    gfx_draw_hline(x+6, cy+4, 12, 0x93C5FD);
    gfx_draw_vline(cx, y+4, 16, 0x93C5FD);
    gfx_fill_circle(cx-3, cy-3, 2, 0xFFFFFF);
    gfx_fill_rect(x+4, y+4, 16, 16, 0x3B82F6);
    gfx_fill_circle(cx, cy, 6, 0x1D4ED8);
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
    gfx_draw_rect_outline(x+3, y+1, 16, 22, 1, 0x9CA3AF);
    gfx_fill_rect(x+1, y+1, 22, 22, 0xE2E8F0);
    gfx_draw_rect_outline(x+1, y+1, 22, 22, 1, 0x94A3B8);
    // Dog-ear
    gfx_fill_rect(x+14, y+1, 5, 5, 0xF1F5F9);
    gfx_draw_line(x+14, y+1, x+14, y+6, 0x9CA3AF);
    gfx_draw_line(x+14, y+6, x+19, y+6, 0x9CA3AF);
    // Margin line
    gfx_draw_vline(x+6, y+3, 18, 0xE5E7EB);
    // Text lines
    gfx_draw_hline(x+8, y+7,  10, 0x3B82F6);
    gfx_draw_hline(x+8, y+10, 10, 0x94A3B8);
    gfx_draw_hline(x+8, y+13, 10, 0x94A3B8);
    gfx_draw_hline(x+8, y+16, 7,  0x94A3B8);
    // Cursor
    gfx_fill_rect(x+8, y+19, 2, 3, 0x10B981);
}

// ── PCI Scanner: circuit-board chip ───────────────────────────────────────
static void draw_pro_pci(int x, int y) {
    gfx_fill_rect(x+4, y+4, 16, 16, 0x166534);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x14532D);
    gfx_fill_rect(x+6, y+6, 12, 12, 0x1A1A2E);
    gfx_draw_rect_outline(x+6, y+6, 12, 12, 1, 0x16213E);
    // Pins left/right
    for(int i=0;i<3;i++) {
        gfx_fill_rect(x+1, y+6+i*4, 3, 2, 0xFBBF24);
        gfx_fill_rect(x+20, y+6+i*4, 3, 2, 0xFBBF24);
    }
    // Top/bottom pins
    for(int i=0;i<3;i++) {
        gfx_fill_rect(x+6+i*4, y+1, 2, 3, 0xFBBF24);
        gfx_fill_rect(x+6+i*4, y+20, 2, 3, 0xFBBF24);
    }
    gfx_fill_circle(x+12, y+12, 3, 0x60A5FA);
    gfx_draw_circle(x+12, y+12, 3, 0x3B82F6);
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
    // Rim highlight
    gfx_draw_hline(x+6, y+4, 12, 0x93C5FD);
    gfx_draw_hline(x+6, y+16, 12, 0x93C5FD);
    // Steam
    gfx_draw_line(x+10, y+2,  x+10, y+0,  0x93C5FD);
    gfx_draw_line(x+14, y+1,  x+14, y-1,  0x93C5FD);
}

// ── System Info: stylized "i" pill badge ───────────────────────────────────
static void draw_pro_system(int x, int y) {
    // Better info icon - rounded square with i
    gfx_fill_rect(x+5, y+3, 14, 18, 0x7C3AED);
    gfx_draw_rect_outline(x+5, y+3, 14, 18, 1, 0x5B21B6);
    // Inner rounded rect
    gfx_fill_rect(x+7, y+5, 10, 14, 0x6D28D9);
    gfx_draw_rect_outline(x+7, y+5, 10, 14, 1, 0x4C1D95);
    // i dot
    gfx_fill_circle(x+12, y+8, 2, 0xFFFFFF);
    // i body
    gfx_fill_rect(x+11, y+11, 2, 6, 0xFFFFFF);
    // Highlight
    gfx_fill_rect(x+8, y+6, 2, 2, 0xA78BFA);
    gfx_draw_hline(x+8, y+15, 8, 0xA78BFA);
}

// ── Clock: simple clock face ──────────────────────────────────────────────
static void draw_pro_clock(int x, int y) {
    gfx_draw_circle(x+12, y+12, 10, 0x34D399);
    gfx_draw_circle(x+12, y+12, 9, 0x10B981);
    gfx_fill_circle(x+12, y+12, 8, 0x0D0D1A);
    // Tick marks
    gfx_draw_hline(x+6, y+4, 12, 0x34D399);
    gfx_draw_hline(x+6, y+20, 12, 0x34D399);
    gfx_draw_vline(x+4, y+6, 12, 0x34D399);
    gfx_draw_vline(x+20, y+6, 12, 0x34D399);
    gfx_draw_hline(x+8, y+5, 8, 0x34D399);
    gfx_draw_hline(x+8, y+19, 8, 0x34D399);
    gfx_draw_vline(x+6, y+8, 8, 0x34D399);
    gfx_draw_vline(x+18, y+8, 8, 0x34D399);
    // Hour hand
    gfx_draw_line(x+12, y+12, x+16, y+8,  0x34D399);
    // Minute hand
    gfx_draw_line(x+12, y+12, x+18, y+12, 0x10B981);
    // Center dot
    gfx_fill_circle(x+12, y+12, 2, 0x34D399);
    gfx_fill_circle(x+12, y+12, 1, 0xFFFFFF);
}

// ── Shutdown: power button ─────────────────────────────────────────────────
static void draw_pro_shutdown(int x, int y) {
    // Thicker arc
    gfx_draw_circle(x+12, y+12, 8, 0xEF4444);
    gfx_draw_circle(x+12, y+12, 7, 0xEF4444);
    // Vertical line
    gfx_fill_rect(x+11, y+4, 3, 7, 0xEF4444);
    gfx_fill_rect(x+12, y+4, 1, 7, 0xFCA5A5);
}

// ── Games: gamepad ────────────────────────────────────────────────────────
static void draw_pro_games(int x, int y) {
    // Main body
    gfx_fill_rect(x+3,  y+8,  18, 8,  0x3B82F6);
    gfx_draw_rect_outline(x+3, y+8, 18, 8,  1, 0x2563EB);
    // Shoulders
    gfx_fill_rect(x+1,  y+9,  3,  6,  0x60A5FA);
    gfx_draw_rect_outline(x+1, y+9, 3, 6,  1, 0x1D4ED8);
    gfx_fill_rect(x+21, y+9,  3,  6,  0x60A5FA);
    gfx_draw_rect_outline(x+21, y+9, 3, 6,  1, 0x1D4ED8);
    // D-pad
    gfx_fill_rect(x+5,  y+9,  3,  2,  0x1E293B);
    gfx_fill_rect(x+4,  y+10, 2,  3,  0x1E293B);
    gfx_fill_rect(x+5,  y+13, 3,  2,  0x1E293B);
    // ABXY buttons
    gfx_fill_circle(x+15, y+9,  2, 0xEF4444);
    gfx_fill_circle(x+18, y+11, 2, 0xFBBF24);
    gfx_fill_circle(x+15, y+13, 2, 0x34D399);
    gfx_fill_circle(x+12, y+11, 2, 0x60A5FA);
    // Start/Select
    gfx_fill_rect(x+10, y+10, 2, 2, 0x9CA3AF);
    gfx_fill_rect(x+13, y+10, 2, 2, 0x9CA3AF);
}

// ── Mines: mine icon ──────────────────────────────────────────────────────
static void draw_pro_mines(int x, int y) {
    // Grid background
    gfx_fill_rect(x+4,  y+4,  16, 16, 0x374151);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0x1E293B);
    // Grid lines
    gfx_draw_hline(x+5, y+8, 14, 0x4B5563);
    gfx_draw_hline(x+5, y+12, 14, 0x4B5563);
    gfx_draw_hline(x+5, y+16, 14, 0x4B5563);
    gfx_draw_vline(x+8, y+5, 14, 0x4B5563);
    gfx_draw_vline(x+12, y+5, 14, 0x4B5563);
    gfx_draw_vline(x+16, y+5, 14, 0x4B5563);
    // Mine in center cell
    gfx_fill_circle(x+12, y+12, 4, 0x111111);
    gfx_draw_circle(x+12, y+12, 4, 0x444444);
    // Spikes
    gfx_draw_line(x+12, y+3,  x+12, y+7,  0x9CA3AF);
    gfx_draw_line(x+12, y+17, x+12, y+21, 0x9CA3AF);
    gfx_draw_line(x+3,  y+12, x+7,  y+12, 0x9CA3AF);
    gfx_draw_line(x+17, y+12, x+21, y+12, 0x9CA3AF);
    gfx_draw_line(x+6,  y+6,  x+9,  y+9,  0x9CA3AF);
    gfx_draw_line(x+18, y+6,  x+15, y+9,  0x9CA3AF);
    gfx_draw_line(x+6,  y+18, x+9,  y+15, 0x9CA3AF);
    gfx_draw_line(x+18, y+18, x+15, y+15, 0x9CA3AF);
    // Highlight
    gfx_fill_circle(x+10, y+10, 2, 0x666666);
}

// ── Pairs: two matching cards ─────────────────────────────────────────────
static void draw_pro_pairs(int x, int y) {
    // Card back
    gfx_fill_rect(x+1,  y+3,  10, 18, 0x1E1E3A);
    gfx_draw_rect_outline(x+1, y+3, 10, 18, 1, 0x111133);
    gfx_fill_rect(x+2,  y+4,  8,  16, 0x252550);
    // Pattern on back
    gfx_fill_circle(x+6, y+10, 2, 0x4B5563);
    gfx_fill_circle(x+6, y+14, 2, 0x4B5563);
    // Right card (Ace)
    gfx_fill_rect(x+13, y+3,  10, 18, 0xF472B6);
    gfx_draw_rect_outline(x+13, y+3, 10, 18, 1, 0xDB2777);
    gfx_fill_rect(x+14, y+4,  8,  16, 0xF9A8D4);
    // A
    gfx_draw_string_transparent(x+16, y+8, "A", 0x9D174D);
    // Separator
    gfx_draw_line(x+11, y+5,  x+11, y+19, 0x10B981);
    gfx_draw_line(x+11, y+5,  x+13, y+7,  0x10B981);
}

// ── Tetris: tetromino stack ───────────────────────────────────────────────
static void draw_pro_tetris(int x, int y) {
    gfx_fill_rect(x+3, y+2, 18, 20, 0x0D0D1A);
    gfx_draw_rect_outline(x+3, y+2, 18, 20, 1, 0x2A2A4A);
    // I piece
    gfx_fill_rect(x+4, y+3, 2, 3, 0x00F0F0);
    gfx_draw_rect_outline(x+4, y+3, 2, 3, 1, 0x088888);
    // L piece
    gfx_fill_rect(x+7, y+3, 2, 3, 0xF0F000);
    gfx_draw_rect_outline(x+7, y+3, 2, 3, 1, 0x888800);
    gfx_fill_rect(x+7, y+6, 2, 2, 0xF0F000);
    // O piece
    gfx_fill_rect(x+10, y+3, 2, 2, 0xA000F0);
    gfx_draw_rect_outline(x+10, y+3, 2, 2, 1, 0x600088);
    gfx_fill_rect(x+12, y+3, 2, 2, 0xA000F0);
    gfx_fill_rect(x+10, y+5, 2, 2, 0xA000F0);
    gfx_fill_rect(x+12, y+5, 2, 2, 0xA000F0);
    // T piece
    gfx_fill_rect(x+15, y+3, 2, 2, 0x00F000);
    gfx_draw_rect_outline(x+15, y+3, 2, 2, 1, 0x008800);
    gfx_fill_rect(x+14, y+5, 2, 2, 0x00F000);
    gfx_fill_rect(x+16, y+5, 2, 2, 0x00F000);
    // S piece
    gfx_fill_rect(x+4, y+8, 2, 2, 0xF00000);
    gfx_fill_rect(x+6, y+8, 2, 2, 0xF00000);
    gfx_fill_rect(x+6, y+10, 2, 2, 0xF00000);
    gfx_fill_rect(x+8, y+10, 2, 2, 0xF00000);
}

// ── Snake: green serpent ──────────────────────────────────────────────────
static void draw_pro_snake(int x, int y) {
    // Background
    gfx_fill_rect(x+4,  y+2,  16, 20, 0x065F46);
    gfx_draw_rect_outline(x+4, y+2, 16, 20, 1, 0x047857);
    // Snake body (more segmented)
    gfx_fill_rect(x+6,  y+4,  4,  2,  0x34D399);
    gfx_fill_rect(x+6,  y+7,  4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+4,  4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+7,  4,  5,  0x34D399);
    gfx_fill_rect(x+6,  y+13, 4,  2,  0x34D399);
    gfx_fill_rect(x+10, y+13, 4,  2,  0x34D399);
    gfx_fill_rect(x+6,  y+16, 8,  2,  0x34D399);
    gfx_fill_rect(x+14, y+16, 4,  2,  0x34D399);
    // Body outline
    gfx_draw_hline(x+6, y+4, 12, 0x047857);
    gfx_draw_hline(x+6, y+16, 12, 0x047857);
    // Eyes
    gfx_fill_rect(x+7,  y+5,  1,  1,  0xFFFFFF);
    gfx_fill_circle(x+11, y+5, 1, 0xFFFFFF);
    gfx_fill_circle(x+11, y+5, 1, 0x000000);
    // Food
    gfx_fill_rect(x+15, y+9,  3,  3,  0xEF4444);
    gfx_draw_rect_outline(x+15, y+9, 3, 3, 1, 0xB91C1C);
}

// ── Piano: keyboard keys ──────────────────────────────────────────────────
static void draw_pro_piano(int x, int y) {
    // Background
    gfx_fill_rect(x+1, y+4, 24, 20, 0x1E293B);
    gfx_draw_rect_outline(x+1, y+4, 24, 20, 1, 0x0F172A);
    // White keys
    for (int i = 0; i < 7; i++) {
        gfx_fill_rect(x+2+i*3, y+6, 2, 16, 0xF8FAFC);
        gfx_draw_rect_outline(x+2+i*3, y+6, 2, 16, 1, 0x94A3B8);
    }
    // Black keys
    int bk[] = {4, 7, 13, 16, 19};
    for (int i = 0; i < 5; i++) {
        gfx_fill_rect(x+bk[i], y+5, 2, 10, 0x1E293B);
        gfx_draw_rect_outline(x+bk[i], y+5, 2, 10, 1, 0x0F172A);
    }
    // Key labels (C, D, E, F, G, A, B)
    gfx_draw_string_transparent(x+3, y+19, "C", 0x9CA3AF);
    gfx_draw_string_transparent(x+6, y+19, "D", 0x9CA3AF);
    gfx_draw_string_transparent(x+9, y+19, "E", 0x9CA3AF);
    gfx_draw_string_transparent(x+12, y+19, "F", 0x9CA3AF);
    gfx_draw_string_transparent(x+15, y+19, "G", 0x9CA3AF);
    gfx_draw_string_transparent(x+18, y+19, "A", 0x9CA3AF);
    gfx_draw_string_transparent(x+21, y+19, "B", 0x9CA3AF);
    // Music note
    gfx_draw_string_transparent(x+2, y+1, "♪", 0x8AB4F8);
}

// ── Accessibility: person icon ──────────────────────────────────────────
static void draw_pro_accessibility(int x, int y) {
    // Better human figure with head, body, arms
    gfx_fill_circle(x+12, y+6,  4, 0xE4E6EA);
    gfx_draw_circle(x+12, y+6,  4, 0x9CA3AF);
    // Eyes
    gfx_fill_circle(x+10, y+5, 1, 0x4B5563);
    gfx_fill_circle(x+14, y+5, 1, 0x4B5563);
    // Smile
    gfx_draw_hline(x+9, y+8, 6, 0x4B5563);
    // Body
    gfx_fill_rect(x+8,  y+11, 8, 6, 0xE4E6EA);
    gfx_draw_rect_outline(x+8, y+11, 8, 6, 1, 0x9CA3AF);
    // Arms
    gfx_draw_line(x+4,  y+12, x+8,  y+13, 0xE4E6EA);
    gfx_draw_line(x+16, y+13, x+20, y+12, 0xE4E6EA);
    gfx_draw_line(x+4,  y+12, x+8,  y+13, 0x9CA3AF);
    gfx_draw_line(x+16, y+13, x+20, y+12, 0x9CA3AF);
    // Vertical line on body
    gfx_draw_vline(x+11, y+11, 6, 0x4B5059);
    // Horizontal lines
    gfx_draw_hline(x+5, y+14, 14, 0xE4E6EA);
    gfx_draw_hline(x+5, y+16, 14, 0xE4E6EA);
    gfx_draw_hline(x+5, y+14, 14, 0x9CA3AF);
    gfx_draw_hline(x+5, y+16, 14, 0x9CA3AF);
}

// ── On-Screen Keyboard icon ──────────────────────────────────────────────
static void draw_pro_osk(int x, int y) {
    // Frame
    gfx_fill_rect(x+3, y+3, 18, 18, 0x1D1F26);
    gfx_draw_rect_outline(x+3, y+3, 18, 18, 1, 0x111318);
    // Keys
    uint32_t kc = 0x4D5059;
    // Row 1
    gfx_fill_rect(x+5, y+5,  2, 2, kc);
    gfx_draw_rect_outline(x+5, y+5, 2, 2, 1, 0x111318);
    gfx_fill_rect(x+8, y+5,  2, 2, kc);
    gfx_fill_rect(x+11, y+5, 2, 2, kc);
    gfx_fill_rect(x+14, y+5, 2, 2, kc);
    gfx_fill_rect(x+17, y+5, 2, 2, kc);
    // Row 2
    gfx_fill_rect(x+5, y+8,  3, 2, kc);
    gfx_fill_rect(x+9, y+8,  2, 2, kc);
    gfx_fill_rect(x+12, y+8, 2, 2, kc);
    gfx_fill_rect(x+15, y+8, 3, 2, kc);
    // Row 3 (space bar)
    gfx_fill_rect(x+7, y+11, 10, 2, kc);
    gfx_draw_rect_outline(x+7, y+11, 10, 2, 1, 0x111318);
    // Enter key highlight
    gfx_fill_rect(x+18, y+8, 2, 2, 0x60A5FA);
}

// ── Weather: sun behind cloud ──────────────────────────────────────────
static void draw_pro_weather(int x, int y) {
    // Larger cloud puffs
    gfx_fill_circle(x+6, y+10, 4, 0xF8FAFC);
    gfx_fill_circle(x+12, y+9, 5, 0xF8FAFC);
    gfx_fill_circle(x+18, y+10, 4, 0xF8FAFC);
    gfx_draw_circle(x+6, y+10, 4, 0x94A3B8);
    gfx_draw_circle(x+12, y+9, 5, 0x94A3B8);
    gfx_draw_circle(x+18, y+10, 4, 0x94A3B8);
    // Cloud base
    gfx_fill_rect(x+4, y+13, 18, 4, 0xF8FAFC);
    gfx_draw_hline(x+4, y+13, 18, 0x94A3B8);
    gfx_draw_hline(x+4, y+17, 18, 0x94A3B8);
    // Sun
    gfx_fill_circle(x+18, y+6, 4, 0xFBBF24);
    gfx_draw_circle(x+18, y+6, 4, 0xF59E0B);
    // Rays
    gfx_draw_line(x+18, y+1, x+18, y+2, 0xF59E0B);
    gfx_draw_line(x+18, y+10, x+18, y+11, 0xF59E0B);
    gfx_draw_line(x+13, y+6, x+14, y+6, 0xF59E0B);
    gfx_draw_line(x+22, y+6, x+23, y+6, 0xF59E0B);
}

// ── Flappy Bird: gold bird silhouette ────────────────────────────────────
static void draw_pro_flappy(int x, int y) {
    // Bird body
    gfx_fill_circle(x+12, y+12, 8, 0xFFD700);
    gfx_draw_circle(x+12, y+12, 8, 0xCC9900);
    // Wing
    gfx_fill_rect(x+8, y+10, 5, 4, 0xE6B800);
    gfx_draw_hline(x+8, y+10, 5, 0xCC9900);
    gfx_draw_hline(x+8, y+14, 5, 0xCC9900);
    // Eye
    gfx_fill_circle(x+15, y+10, 3, 0xFFFFFF);
    gfx_draw_circle(x+15, y+10, 3, 0xE5E7EB);
    gfx_fill_circle(x+16, y+10, 1, 0x000000);
    // Beak
    gfx_fill_rect(x+18, y+11, 4, 2, 0xFF8C00);
    gfx_draw_hline(x+18, y+11, 4, 0xCC5500);
    gfx_draw_hline(x+18, y+13, 4, 0xCC5500);
    // Tail
    gfx_fill_rect(x+5, y+13, 3, 2, 0xE6B800);
}

// ── 2048: game tile ──────────────────────────────────────────────────────
static void draw_pro_2048(int x, int y) {
    // Background
    gfx_fill_rect(x+2, y+2, 20, 20, 0xEDC552);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0xB58A1E);
    // Inner tile
    gfx_fill_rect(x+4, y+4, 16, 16, 0xF59E0B);
    gfx_draw_rect_outline(x+4, y+4, 16, 16, 1, 0xD97706);
    // Inner inner tile
    gfx_fill_rect(x+6, y+6, 12, 12, 0xE3B341);
    gfx_draw_rect_outline(x+6, y+6, 12, 12, 1, 0xB58A1E);
    gfx_draw_string_transparent(x+7, y+10, "2048", 0xFFFFFF);
    gfx_draw_string_transparent(x+8, y+11, "2048", 0xFFFFFF44);
    // Corner highlights
    gfx_fill_rect(x+2, y+2, 4, 1, 0xFCD34D);
    gfx_fill_rect(x+2, y+2, 1, 4, 0xFCD34D);
}
static void draw_pro_imgview(int x, int y) {
    // Frame
    gfx_fill_rect(x+2, y+2, 20, 20, 0x4B5563);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0x1F2937);
    // Screen area with landscape
    gfx_fill_rect(x+3, y+3, 18, 14, 0x0A0D16);
    // Sky gradient effect (darker at top, lighter at bottom)
    gfx_fill_rect(x+3, y+3, 18, 3, 0x111827);
    gfx_fill_rect(x+3, y+6, 18, 4, 0x1D4ED8);
    // Sun
    gfx_fill_circle(x+8, y+5, 2, 0xFBBF24);
    gfx_draw_circle(x+8, y+5, 2, 0xF59E0B);
    // Mountains
    gfx_fill_rect(x+3, y+9, 6, 5, 0x065F46);
    gfx_draw_rect_outline(x+3, y+9, 6, 5, 1, 0x047857);
    gfx_fill_rect(x+9, y+10, 7, 4, 0x065F46);
    gfx_draw_rect_outline(x+9, y+10, 7, 4, 1, 0x047857);
    gfx_fill_rect(x+16, y+9, 5, 5, 0x065F46);
    gfx_draw_rect_outline(x+16, y+9, 5, 5, 1, 0x047857);
    // Ground
    gfx_fill_rect(x+3, y+14, 18, 3, 0x065F46);
    gfx_draw_hline(x+3, y+14, 18, 0x047857);
    // Bottom controls
    gfx_fill_rect(x+5, y+18, 14, 2, 0x1D1F26);
    gfx_draw_rect_outline(x+5, y+18, 14, 2, 1, 0x4D5059);
}

// ── Sudoku: 3x3 grid with one centered digit ─────────────────────────────
static void draw_pro_sudoku(int x, int y) {
    // Background
    gfx_fill_rect(x+2, y+2, 20, 20, 0xE2E8F0);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0x475569);
    gfx_fill_rect(x+3, y+3, 18, 18, 0xF1F5F9);
    // Grid lines (thin)
    gfx_draw_hline(x+3, y+6, 18, 0x94A3B8);
    gfx_draw_hline(x+3, y+12, 18, 0x94A3B8);
    gfx_draw_hline(x+3, y+18, 18, 0x94A3B8);
    gfx_draw_vline(x+6, y+3, 18, 0x94A3B8);
    gfx_draw_vline(x+12, y+3, 18, 0x94A3B8);
    gfx_draw_vline(x+18, y+3, 18, 0x94A3B8);
    // Bold 3x3 borders
    gfx_draw_hline(x+3, y+9, 18, 0x1E293B);
    gfx_draw_hline(x+3, y+15, 18, 0x1E293B);
    gfx_draw_vline(x+9, y+3, 18, 0x1E293B);
    gfx_draw_vline(x+15, y+3, 18, 0x1E293B);
    // Corner 3x3 border
    gfx_draw_rect_outline(x+3, y+3, 18, 18, 1, 0x475569);
    // Center digit
    gfx_draw_string_transparent(x+9, y+8, "5", 0x1E293B);
    gfx_draw_string_transparent(x+15, y+8, "3", 0x1E293B);
    gfx_draw_string_transparent(x+9, y+14, "7", 0x1E293B);
    gfx_draw_string_transparent(x+15, y+14, "9", 0x1E293B);
}

// ── Graphics: picture frame with landscape ────────────────────────────────
static void draw_pro_graphics(int x, int y) {
    // Frame
    gfx_fill_rect(x+2, y+2, 20, 20, 0x374151);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0x1E293B);
    // Canvas area
    gfx_fill_rect(x+3, y+3, 18, 15, 0x0A0D16);
    // Landscape - sky
    gfx_fill_rect(x+3, y+3, 18, 6, 0x3B82F6);
    gfx_fill_circle(x+8, y+6, 2, 0xFBBF24);
    // Landscape - ground
    gfx_fill_rect(x+3, y+9, 18, 9, 0x065F46);
    gfx_draw_hline(x+3, y+9, 18, 0x047857);
    // Mountains
    gfx_fill_rect(x+5, y+8, 5, 4, 0x22D3EE);
    gfx_fill_rect(x+10, y+9, 4, 3, 0x22D3EE);
    gfx_fill_rect(x+14, y+8, 5, 4, 0x22D3EE);
    // Trees
    gfx_fill_rect(x+4, y+11, 2, 3, 0x065F46);
    gfx_fill_rect(x+16, y+11, 2, 3, 0x065F46);
    // Bottom toolbar
    gfx_fill_rect(x+3, y+18, 18, 2, 0x1D1F26);
    gfx_draw_rect_outline(x+3, y+18, 18, 2, 1, 0x4D5059);
    // Tool icons on toolbar
    gfx_fill_rect(x+5, y+19, 2, 1, 0x60A5FA);
    gfx_fill_rect(x+8, y+19, 2, 1, 0xFBBF24);
    gfx_fill_circle(x+12, y+19, 1, 0x34D399);
    gfx_fill_rect(x+15, y+19, 2, 1, 0xF87171);
}


static void draw_pro_mandelbrot(int x, int y) {
    // Fractal-like icon: colorful nested squares
    gfx_fill_rect(x+2, y+2, 20, 20, 0x0A0D16);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, 0x1E293B);
    gfx_fill_rect(x+4, y+4, 16, 16, 0x7C3AED);
    gfx_fill_rect(x+6, y+6, 12, 12, 0x2563EB);
    gfx_fill_rect(x+8, y+8,  8,  8, 0x059669);
    gfx_fill_rect(x+10, y+10, 4, 4, 0xFBBF24);
}

// ── Dispatch ──────────────────────────────────────────────────────────────
void draw_app_icon(const char* name, int x, int y) {
    if (!name) goto fallback;
    // Match by first 4 chars
    if      (name[0]=='C'&&name[1]=='a'&&name[2]=='l'&&name[3]=='e') draw_pro_calendar(x, y);
    else if (name[0]=='C'&&name[1]=='a'&&name[2]=='l'&&name[3]=='c') draw_pro_calc(x, y);
    else if (name[0]=='F'&&name[1]=='i') draw_pro_folder(x, y);
    else if (name[0]=='F'&&name[1]=='l') draw_pro_flappy(x, y);
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
    else if (name[0]=='I'&&name[1]=='m') draw_pro_imgview(x, y);  // "Image Viewer"
    else if (name[0]=='P'&&name[1]=='i') draw_pro_piano(x, y);    // "Piano"
    else if (name[0]=='S'&&name[1]=='n') draw_pro_snake(x, y);    // "Snake"
    else if (name[0]=='M'&&name[1]=='i') draw_pro_mines(x, y);    // "Mines"
    else if (name[0]=='W') draw_pro_weather(x, y);                // "Weather"
    else if (name[0]=='2') draw_pro_2048(x, y);                   // "2048"
    else if (name[0]=='S'&&name[1]=='u') draw_pro_sudoku(x, y);   // "Sudoku"
    else if (name[0]=='G'&&name[1]=='r') draw_pro_graphics(x, y); // "Graphics"
    else if (name[0]=='M'&&name[1]=='a') draw_pro_mandelbrot(x, y); // "Mandelbrot"
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

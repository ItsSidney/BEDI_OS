#include "gui/app_icons.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"

static void icon_bg_3d(int x, int y, uint32_t base, uint32_t light, uint32_t dark) {
    gfx_fill_rect(x+2, y+3, 20, 20, dark);
    gfx_fill_rect(x+2, y+2, 20, 20, base);
    gfx_fill_rect(x+2, y+2, 20, 2, light);
    gfx_fill_rect(x+2, y+20, 20, 1, dark);
    gfx_draw_rect_outline(x+2, y+2, 20, 20, 1, dark);
}

static void icon_bg_3d_round(int x, int y, uint32_t base, uint32_t light, uint32_t dark) {
    gfx_fill_rect(x+3, y+4, 18, 18, dark);
    gfx_fill_rect(x+3, y+3, 18, 18, base);
    gfx_fill_rect(x+3, y+3, 18, 2, light);
    gfx_fill_rect(x+3, y+19, 18, 1, dark);
    gfx_draw_rect_rounded_outline(x+3, y+3, 18, 18, 4, 1, dark);
}

static void icon_highlight(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, 0xFFFFFF);
}

static void bevel_box(int x, int y, int w, int h, uint32_t base, uint32_t light, uint32_t dark) {
    gfx_fill_rect(x+1, y+1, w-2, h-2, base);
    gfx_fill_rect(x, y, w, 1, light);
    gfx_fill_rect(x, y+h-1, w, 1, dark);
    gfx_fill_rect(x, y, 1, h, light);
    gfx_fill_rect(x+w-1, y, 1, h, dark);
}

static void bevel_circle(int cx, int cy, int r, uint32_t base, uint32_t light, uint32_t dark) {
    gfx_fill_circle(cx, cy+1, r, dark);
    gfx_fill_circle(cx, cy, r, base);
    gfx_fill_circle(cx-1, cy-1, r-1, light);
}

static void draw_icon_calc(int x, int y) {
    icon_bg_3d_round(x, y, 0x7C3AED, 0xA78BFA, 0x5B21B6);
    bevel_box(x+5, y+6, 14, 5, 0x1E1B4B, 0x312E81, 0x0F0A2E);
    gfx_draw_string_transparent(x+7, y+7, "0.", 0xFFFFFF);
    gfx_draw_string_transparent(x+13, y+7, "0", 0xE9D5FF);
    uint32_t kc[4][3] = {{0xFBBF24,0xF59E0B,0xD97706},{0x34D399,0x10B981,0x059669},{0x60A5FA,0x3B82F6,0x2563EB},{0xF87171,0xEF4444,0xDC2626}};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            int px = x+5+c*4, py = y+13+r*3;
            gfx_fill_rect(px+1, py+1, 3, 2, kc[r*2+c][2]);
            gfx_fill_rect(px, py, 3, 2, kc[r*2+c][0]);
        }
}

static void draw_icon_folder(int x, int y) {
    icon_bg_3d(x, y, 0xF59E0B, 0xFCD34D, 0xD97706);
    gfx_fill_rect(x+4, y+6, 16, 14, 0xFEF3C7);
    bevel_box(x+4, y+6, 16, 14, 0xFDE68A, 0xFEF3C7, 0xFCD34D);
    gfx_fill_rect(x+4, y+6, 8, 3, 0xFFFBEB);
    gfx_fill_rect(x+5, y+11, 14, 2, 0xFBBF24);
    gfx_fill_rect(x+5, y+15, 10, 2, 0xFBBF24);
}

static void draw_icon_terminal(int x, int y) {
    icon_bg_3d_round(x, y, 0x0F172A, 0x1E293B, 0x020617);
    bevel_box(x+4, y+4, 16, 16, 0x0F172A, 0x1E293B, 0x020617);
    gfx_draw_string_transparent(x+7, y+8, ">_", 0x34D399);
    gfx_draw_hline(x+6, y+12, 12, 0x334155);
    gfx_draw_hline(x+6, y+15, 8, 0x334155);
    bevel_circle(x+8, y+18, 2, 0xEF4444, 0xFCA5A5, 0xB91C1C);
    bevel_circle(x+13, y+18, 2, 0xFBBF24, 0xFDE68A, 0xD97706);
    bevel_circle(x+18, y+18, 2, 0x34D399, 0x6EE7B7, 0x059669);
}

static void draw_icon_calendar(int x, int y) {
    icon_bg_3d_round(x, y, 0xFFFFFF, 0xFFFFFF, 0xD1D5DB);
    bevel_box(x+5, y+5, 14, 14, 0xF8FAFC, 0xFFFFFF, 0xE5E7EB);
    gfx_fill_rect(x+5, y+5, 14, 5, 0xEF4444);
    gfx_fill_rect(x+5, y+5, 14, 1, 0xFCA5A5);
    gfx_draw_string_transparent(x+8, y+12, "23", 0x111827);
    gfx_draw_vline(x+12, y+4, 3, 0x9CA3AF);
    gfx_draw_vline(x+8, y+4, 3, 0x9CA3AF);
    gfx_draw_vline(x+16, y+4, 3, 0x9CA3AF);
}

static void draw_icon_httpviewer(int x, int y) {
    icon_bg_3d_round(x, y, 0x1D4ED8, 0x3B82F6, 0x1E3A8A);
    bevel_circle(x+12, y+12, 7, 0x2563EB, 0x60A5FA, 0x1D4ED8);
    gfx_draw_circle(x+12, y+12, 6, 0x93C5FD);
    gfx_draw_hline(x+6, y+12, 12, 0x93C5FD);
    gfx_draw_vline(x+12, y+6, 12, 0x93C5FD);
    gfx_fill_circle(x+9, y+9, 2, 0xFFFFFF);
}

static void draw_icon_process(int x, int y) {
    icon_bg_3d_round(x, y, 0x0F172A, 0x1E293B, 0x020617);
    bevel_box(x+5, y+5, 14, 14, 0x1E293B, 0x334155, 0x0F172A);
    gfx_draw_line(x+6, y+12, x+10, y+8, 0x10B981);
    gfx_draw_line(x+10, y+8, x+13, y+16, 0x10B981);
    gfx_draw_line(x+13, y+16, x+17, y+10, 0x10B981);
    bevel_circle(x+10, y+8, 2, 0x10B981, 0x6EE7B7, 0x047857);
    bevel_circle(x+13, y+16, 2, 0xEF4444, 0xFCA5A5, 0xB91C1C);
    gfx_fill_circle(x+17, y+10, 1, 0xFFFFFF);
}

static void draw_icon_text(int x, int y) {
    icon_bg_3d_round(x, y, 0xE2E8F0, 0xF1F5F9, 0x94A3B8);
    bevel_box(x+5, y+5, 14, 14, 0xF8FAFC, 0xFFFFFF, 0xE2E8F0);
    gfx_fill_rect(x+5, y+5, 3, 14, 0x3B82F6);
    gfx_fill_rect(x+5, y+5, 3, 1, 0x93C5FD);
    gfx_draw_hline(x+10, y+8, 7, 0x94A3B8);
    gfx_draw_hline(x+10, y+11, 5, 0x94A3B8);
    gfx_draw_hline(x+10, y+14, 6, 0x94A3B8);
    gfx_fill_rect(x+10, y+17, 2, 2, 0x10B981);
}

static void draw_icon_pci(int x, int y) {
    icon_bg_3d_round(x, y, 0x14532D, 0x166534, 0x064E3B);
    bevel_box(x+5, y+5, 14, 14, 0x0D1117, 0x1E293B, 0x000000);
    for (int i = 0; i < 3; i++) {
        bevel_box(x+2, y+7+i*3, 4, 3, 0xFBBF24, 0xFDE68A, 0xD97706);
        bevel_box(x+18, y+7+i*3, 4, 3, 0xFBBF24, 0xFDE68A, 0xD97706);
    }
    for (int i = 0; i < 3; i++) {
        bevel_box(x+7+i*3, y+2, 3, 4, 0xFBBF24, 0xFDE68A, 0xD97706);
        bevel_box(x+7+i*3, y+18, 3, 4, 0xFBBF24, 0xFDE68A, 0xD97706);
    }
    gfx_fill_circle(x+12, y+12, 4, 0x60A5FA);
    gfx_fill_circle(x+12, y+12, 3, 0x93C5FD);
    gfx_fill_circle(x+12, y+12, 2, 0xFFFFFF);
}

static void draw_icon_colour(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E293B, 0x334155, 0x0F172A);
    bevel_circle(x+12, y+12, 8, 0x7C3AED, 0xA78BFA, 0x5B21B6);
    bevel_circle(x+12, y+12, 6, 0xEC4899, 0xF472B6, 0xBE185D);
    bevel_circle(x+12, y+12, 4, 0x3B82F6, 0x60A5FA, 0x1D4ED8);
    bevel_circle(x+12, y+12, 2, 0xFBBF24, 0xFDE68A, 0xD97706);
}

static void draw_icon_system(int x, int y) {
    icon_bg_3d_round(x, y, 0x5B21B6, 0x7C3AED, 0x3B0764);
    bevel_circle(x+12, y+12, 7, 0x6D28D9, 0x8B5CF6, 0x4C1D95);
    bevel_circle(x+12, y+12, 5, 0x7C3AED, 0xA78BFA, 0x5B21B6);
    gfx_fill_circle(x+12, y+12, 2, 0xFFFFFF);
    gfx_fill_rect(x+10, y+5, 4, 3, 0xFFFFFF);
    gfx_fill_rect(x+10, y+17, 4, 2, 0xFFFFFF);
    gfx_fill_rect(x+5, y+10, 3, 4, 0xFFFFFF);
    gfx_fill_rect(x+17, y+10, 2, 4, 0xFFFFFF);
}

static void draw_icon_shutdown(int x, int y) {
    icon_bg_3d_round(x, y, 0xDC2626, 0xEF4444, 0x991B1B);
    gfx_fill_rect(x+11, y+6, 4, 7, 0xFFFFFF);
    gfx_fill_rect(x+11, y+6, 4, 1, 0xFCA5A5);
    gfx_draw_circle(x+13, y+12, 6, 0xFFFFFF);
    gfx_fill_circle(x+13, y+12, 5, 0xDC2626);
    gfx_fill_circle(x+13, y+12, 4, 0xEF4444);
}

static void draw_icon_games(int x, int y) {
    icon_bg_3d(x, y, 0x2563EB, 0x3B82F6, 0x1D4ED8);
    bevel_box(x+4, y+9, 16, 9, 0x1E293B, 0x334155, 0x0F172A);
    gfx_fill_rect(x+3, y+10, 3, 7, 0x60A5FA);
    gfx_fill_rect(x+18, y+10, 3, 7, 0x60A5FA);
    bevel_circle(x+9, y+13, 2, 0xEF4444, 0xFCA5A5, 0xB91C1C);
    bevel_circle(x+15, y+15, 2, 0xFBBF24, 0xFDE68A, 0xD97706);
    bevel_circle(x+12, y+13, 2, 0x34D399, 0x6EE7B7, 0x059669);
    bevel_circle(x+15, y+11, 2, 0x60A5FA, 0x93C5FD, 0x2563EB);
    gfx_fill_rect(x+10, y+12, 2, 2, 0x94A3B8);
}

static void draw_icon_mines(int x, int y) {
    icon_bg_3d_round(x, y, 0x374151, 0x4B5563, 0x1F2937);
    bevel_box(x+5, y+5, 14, 14, 0x1E293B, 0x334155, 0x0F172A);
    gfx_draw_hline(x+5, y+9, 14, 0x334155);
    gfx_draw_hline(x+5, y+13, 14, 0x334155);
    gfx_draw_vline(x+9, y+5, 14, 0x334155);
    gfx_draw_vline(x+15, y+5, 14, 0x334155);
    bevel_circle(x+12, y+12, 3, 0x111111, 0x374151, 0x000000);
    gfx_draw_circle(x+12, y+12, 3, 0xEF4444);
    gfx_draw_line(x+12, y+5, x+12, y+8, 0x9CA3AF);
    gfx_draw_line(x+12, y+16, x+12, y+19, 0x9CA3AF);
    gfx_draw_line(x+5, y+12, x+8, y+12, 0x9CA3AF);
    gfx_draw_line(x+16, y+12, x+19, y+12, 0x9CA3AF);
}

static void draw_icon_tetris(int x, int y) {
    icon_bg_3d(x, y, 0x1E1E3A, 0x2A2A4A, 0x0D0D1A);
    uint32_t colors[4][3] = {{0x00D4D4,0x00F0F0,0x00A0A0},{0xF0F000,0xFFFF40,0xC0C000},{0xA000F0,0xC040FF,0x7000B0},{0xF00000,0xFF4040,0xB00000}};
    int bx[4] = {5,9,13,7}, by[4] = {5,7,5,11}, bw[4] = {4,4,5,4}, bh[4] = {5,4,5,4};
    int ci[4] = {0,1,2,3};
    for (int i = 0; i < 4; i++) {
        int px = x+bx[i], py = y+by[i];
        bevel_box(px, py, bw[i], bh[i], colors[ci[i]][0], colors[ci[i]][1], colors[ci[i]][2]);
    }
}

static void draw_icon_pairs(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E1E3A, 0x2A2A4A, 0x0D0D1A);
    bevel_box(x+4, y+5, 8, 16, 0x4B5563, 0x6B7280, 0x374151);
    bevel_circle(x+8, y+13, 2, 0xFFFFFF, 0xFFFFFF, 0xD1D5DB);
    bevel_box(x+12, y+5, 8, 16, 0xA855F7, 0xC084FC, 0x7E22CE);
    gfx_draw_string_transparent(x+15, y+13, "♥", 0xFFFFFF);
    gfx_draw_line(x+11, y+7, x+12, y+9, 0x10B981);
    gfx_draw_line(x+11, y+19, x+12, y+17, 0x10B981);
}

static void draw_icon_2048(int x, int y) {
    icon_bg_3d_round(x, y, 0xEDC552, 0xFDE68A, 0xB58A1E);
    bevel_box(x+5, y+5, 14, 14, 0xF59E0B, 0xFBBF24, 0xD97706);
    bevel_box(x+7, y+7, 10, 10, 0xE3B341, 0xFDE68A, 0xB58A1E);
    gfx_draw_string_transparent(x+8, y+10, "2", 0xFFFFFF);
    gfx_draw_string_transparent(x+12, y+13, "8", 0xFFFFFF);
}

static void draw_icon_sudoku(int x, int y) {
    icon_bg_3d_round(x, y, 0xF1F5F9, 0xFFFFFF, 0x94A3B8);
    bevel_box(x+5, y+5, 14, 14, 0xF8FAFC, 0xFFFFFF, 0xE2E8F0);
    gfx_draw_hline(x+5, y+9, 14, 0x475569);
    gfx_draw_hline(x+5, y+13, 14, 0x475569);
    gfx_draw_vline(x+9, y+5, 14, 0x475569);
    gfx_draw_vline(x+13, y+5, 14, 0x475569);
    gfx_draw_hline(x+5, y+11, 14, 0x1E293B);
    gfx_draw_vline(x+11, y+5, 14, 0x1E293B);
    gfx_draw_string_transparent(x+7, y+8, "5", 0x1E293B);
    gfx_draw_string_transparent(x+12, y+13, "9", 0x1E293B);
    gfx_fill_rect(x+7, y+7, 2, 2, 0xEF4444);
    gfx_fill_rect(x+14, y+16, 2, 2, 0xEF4444);
}

static void draw_icon_snake(int x, int y) {
    icon_bg_3d_round(x, y, 0x065F46, 0x059669, 0x022C22);
    gfx_fill_rect(x+5, y+5, 14, 14, 0x047857);
    bevel_box(x+5, y+5, 14, 14, 0x047857, 0x059669, 0x065F46);
    bevel_box(x+5, y+10, 5, 4, 0x34D399, 0x6EE7B7, 0x10B981);
    bevel_box(x+11, y+8, 5, 4, 0x10B981, 0x34D399, 0x047857);
    bevel_box(x+15, y+10, 4, 4, 0x34D399, 0x6EE7B7, 0x10B981);
    bevel_box(x+5, y+15, 5, 4, 0x34D399, 0x6EE7B7, 0x10B981);
    gfx_fill_rect(x+5, y+19, 8, 2, 0x059669);
    bevel_circle(x+17, y+10, 2, 0xEF4444, 0xFCA5A5, 0xB91C1C);
    gfx_fill_rect(x+8, y+12, 2, 1, 0xFFFFFF);
    gfx_fill_rect(x+8, y+12, 1, 2, 0x000000);
}

static void draw_icon_flappy(int x, int y) {
    icon_bg_3d_round(x, y, 0xEA580C, 0xF97316, 0x9A3412);
    bevel_circle(x+12, y+12, 7, 0xFBBF24, 0xFDE68A, 0xD97706);
    gfx_fill_rect(x+8, y+10, 4, 4, 0xEAB308);
    gfx_fill_circle(x+15, y+10, 2, 0xFFFFFF);
    gfx_fill_circle(x+15, y+10, 1, 0x000000);
    bevel_box(x+17, y+12, 4, 3, 0xF97316, 0xFB923C, 0xEA580C);
    bevel_box(x+5, y+14, 4, 3, 0xEAB308, 0xFDE68A, 0xD97706);
    gfx_draw_hline(x+10, y+5, 4, 0xFBBF24);
    gfx_draw_hline(x+12, y+20, 5, 0xF97316);
}

static void draw_icon_clock(int x, int y) {
    icon_bg_3d_round(x, y, 0x0D0D1A, 0x1E1E3A, 0x000000);
    bevel_circle(x+12, y+12, 9, 0x10B981, 0x34D399, 0x047857);
    gfx_fill_circle(x+12, y+12, 8, 0x0D0D1A);
    gfx_draw_hline(x+6, y+4, 12, 0x10B981);
    gfx_draw_hline(x+6, y+20, 12, 0x10B981);
    gfx_draw_vline(x+4, y+7, 10, 0x10B981);
    gfx_draw_vline(x+20, y+7, 10, 0x10B981);
    gfx_draw_line(x+12, y+12, x+16, y+8, 0xFBBF24);
    gfx_draw_line(x+12, y+12, x+14, y+16, 0x34D399);
    bevel_circle(x+12, y+12, 2, 0xFFFFFF, 0xFFFFFF, 0x9CA3AF);
    bevel_circle(x+12, y+12, 1, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF);
}

static void draw_icon_bitmap_maker(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E293B, 0x334155, 0x0F172A);
    int ps = 4;
    int ox = x+5, oy = y+5;
    uint32_t colors[] = {0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFBBF24,0x3FB950,0x58A6FF,0xBC8CFF,0xF85149,0xF0883E,0x39D2C0,0xF778BA,0xFFFFFF,0x8B949E,0x6D7079,0x0D0E12};
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            uint32_t col = colors[r*4+c];
            gfx_fill_rect(ox+c*ps, oy+r*ps, ps-1, ps-1, col);
            gfx_fill_rect(ox+c*ps, oy+r*ps, ps-2, 1, gfx_lighten(col, 60));
            gfx_fill_rect(ox+c*ps, oy+r*ps, 1, ps-2, gfx_lighten(col, 60));
        }
    bevel_box(x+20, y+13, 6, 4, 0xFBBF24, 0xFDE68A, 0xD97706);
    gfx_fill_rect(x+24, y+13, 2, 4, 0xFFFFFF);
}

static void draw_icon_piano(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E293B, 0x334155, 0x0F172A);
    for (int i = 0; i < 7; i++)
        bevel_box(x+4+i*2, y+7, 2, 12, 0xF8FAFC, 0xFFFFFF, 0xD1D5DB);
    int bk[] = {7,10,15,18};
    for (int i = 0; i < 4; i++)
        bevel_box(x+bk[i], y+5, 2, 8, 0x1E293B, 0x334155, 0x0F172A);
    gfx_draw_string_transparent(x+5, y+19, "♪", 0x8AB4F8);
}

static void draw_icon_accessibility(int x, int y) {
    icon_bg_3d_round(x, y, 0xD1D5DB, 0xE5E7EB, 0x9CA3AF);
    bevel_circle(x+12, y+8, 4, 0x4B5563, 0x6B7280, 0x374151);
    gfx_draw_hline(x+9, y+12, 6, 0x4B5563);
    bevel_box(x+7, y+14, 10, 6, 0x4B5563, 0x6B7280, 0x374151);
    gfx_draw_hline(x+8, y+16, 8, 0x1E293B);
    gfx_draw_hline(x+8, y+18, 4, 0x1E293B);
    gfx_fill_rect(x+12, y+18, 2, 2, 0xFFFFFF);
}

static void draw_icon_osk(int x, int y) {
    icon_bg_3d_round(x, y, 0x1D1F26, 0x2D2F36, 0x0D0F16);
    bevel_box(x+4, y+4, 16, 16, 0x252830, 0x353840, 0x151820);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 5; c++) {
            int kx = x+5+c*2, ky = y+7+r*3;
            bevel_box(kx, ky, 2, 2, 0x4D5059, 0x6D7079, 0x3D4049);
        }
    bevel_box(x+7, y+14, 10, 2, 0x3B82F6, 0x60A5FA, 0x2563EB);
}

static void draw_icon_imgview(int x, int y) {
    icon_bg_3d_round(x, y, 0x374151, 0x4B5563, 0x1F2937);
    bevel_box(x+4, y+4, 16, 16, 0x0F172A, 0x1E293B, 0x020617);
    gfx_fill_rect(x+4, y+4, 16, 5, 0x2563EB);
    bevel_circle(x+11, y+9, 2, 0xFBBF24, 0xFDE68A, 0xD97706);
    gfx_fill_rect(x+4, y+15, 8, 5, 0x065F46);
    gfx_fill_rect(x+12, y+15, 8, 5, 0x047857);
    gfx_draw_vline(x+18, y+16, 3, 0x10B981);
}

static void draw_icon_hexdump(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E293B, 0x334155, 0x0F172A);
    bevel_box(x+4, y+4, 16, 16, 0x0F141D, 0x1A1F28, 0x050A13);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            int hx = x+5+c*3+1, hy = y+7+r*3;
            bevel_box(hx, hy, 2, 2, 0x3FB950, 0x6EE7B7, 0x15803D);
        }
    bevel_box(x+6, y+10, 2, 2, 0x58A6FF, 0x93C5FD, 0x2563EB);
    bevel_box(x+9, y+10, 2, 2, 0xF0883E, 0xFB923C, 0xC2410C);
    bevel_box(x+15, y+10, 2, 2, 0xBC8CFF, 0xD8B4FE, 0x7E22CE);
    gfx_draw_string_transparent(x+5, y+12, "0x1A", 0xE6EDF3);
}

static void draw_icon_mandelbrot(int x, int y) {
    icon_bg_3d_round(x, y, 0x0A0D16, 0x1A1D26, 0x000000);
    bevel_box(x+5, y+5, 14, 14, 0x7C3AED, 0x8B5CF6, 0x5B21B6);
    bevel_box(x+7, y+7, 10, 10, 0x2563EB, 0x3B82F6, 0x1D4ED8);
    bevel_box(x+9, y+9, 6, 6, 0x059669, 0x10B981, 0x047857);
    bevel_box(x+11, y+11, 2, 2, 0xFBBF24, 0xFDE68A, 0xD97706);
}

static void draw_icon_personalization(int x, int y) {
    icon_bg_3d_round(x, y, 0x6D28D9, 0x7C3AED, 0x4C1D95);
    bevel_circle(x+9, y+9, 4, 0xEC4899, 0xF472B6, 0xBE185D);
    bevel_circle(x+15, y+9, 4, 0x3B82F6, 0x60A5FA, 0x1D4ED8);
    bevel_circle(x+9, y+15, 4, 0xFBBF24, 0xFDE68A, 0xD97706);
    bevel_circle(x+15, y+15, 4, 0x10B981, 0x34D399, 0x047857);
    bevel_circle(x+12, y+12, 3, 0xFFFFFF, 0xFFFFFF, 0xD1D5DB);
}

static void draw_icon_debug(int x, int y) {
    icon_bg_3d_round(x, y, 0xBE185D, 0xDB2777, 0x831843);
    bevel_box(x+7, y+7, 10, 11, 0xEC4899, 0xF472B6, 0xBE185D);
    bevel_box(x+5, y+5, 14, 6, 0xBE185D, 0xDB2777, 0x831843);
    gfx_fill_rect(x+7, y+5, 10, 3, 0xF472B6);
    gfx_fill_rect(x+8, y+10, 2, 2, 0xFFFFFF);
    gfx_fill_rect(x+14, y+10, 2, 2, 0xFFFFFF);
    gfx_fill_rect(x+5, y+15, 5, 3, 0xF472B6);
    gfx_fill_rect(x+14, y+15, 5, 3, 0xF472B6);
    gfx_fill_rect(x+2, y+8, 4, 3, 0xF472B6);
    gfx_fill_rect(x+18, y+8, 4, 3, 0xF472B6);
    gfx_fill_rect(x+10, y+3, 2, 2, 0xF472B6);
    gfx_fill_rect(x+12, y+2, 2, 2, 0xF472B6);
}

static void draw_icon_perfmon(int x, int y) {
    icon_bg_3d_round(x, y, 0x0F172A, 0x1E293B, 0x020617);
    bevel_box(x+4, y+13, 6, 6, 0x58A6FF, 0x93C5FD, 0x2563EB);
    bevel_box(x+11, y+9, 6, 10, 0x3FB950, 0x6EE7B7, 0x15803D);
    bevel_box(x+18, y+5, 6, 14, 0xF0883E, 0xFB923C, 0xC2410C);
    gfx_draw_hline(x+4, y+11, 20, 0x1E293B);
    gfx_draw_hline(x+4, y+16, 20, 0x1E293B);
    gfx_fill_circle(x+7, y+16, 1, 0xFFFFFF);
    gfx_fill_circle(x+14, y+12, 1, 0xFFFFFF);
    gfx_fill_circle(x+21, y+8, 1, 0xFFFFFF);
}

static void draw_icon_kernellog(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E293B, 0x334155, 0x0F172A);
    bevel_box(x+4, y+4, 16, 16, 0x0F172A, 0x1E293B, 0x020617);
    gfx_draw_string_transparent(x+6, y+7, ">", 0x34D399);
    gfx_draw_hline(x+6, y+11, 12, 0x334155);
    gfx_draw_hline(x+6, y+14, 10, 0x334155);
    gfx_draw_hline(x+6, y+17, 8, 0x334155);
    bevel_box(x+18, y+6, 3, 3, 0x34D399, 0x6EE7B7, 0x059669);
    bevel_box(x+18, y+10, 3, 3, 0xFBBF24, 0xFDE68A, 0xD97706);
    bevel_box(x+18, y+14, 3, 3, 0xEF4444, 0xFCA5A5, 0xB91C1C);
}

static void draw_icon_netdebug(int x, int y) {
    icon_bg_3d_round(x, y, 0x1E3A5F, 0x2563EB, 0x0F2557);
    bevel_box(x+4, y+4, 16, 16, 0x1D4ED8, 0x3B82F6, 0x1E40AF);
    gfx_draw_hline(x+6, y+8, 12, 0x60A5FA);
    gfx_draw_hline(x+6, y+11, 12, 0x60A5FA);
    gfx_draw_hline(x+6, y+14, 12, 0x60A5FA);
    gfx_fill_circle(x+7, y+11, 1, 0x93C5FD);
    gfx_fill_circle(x+14, y+8, 1, 0x93C5FD);
    gfx_fill_circle(x+14, y+14, 1, 0x93C5FD);
    gfx_draw_line(x+10, y+12, x+14, y+8, 0xBFDBFE);
    gfx_draw_line(x+10, y+13, x+14, y+14, 0xBFDBFE);
    gfx_draw_line(x+15, y+10, x+18, y+8, 0xBFDBFE);
    bevel_box(x+18, y+5, 2, 12, 0x3B82F6, 0x60A5FA, 0x1D4ED8);
}

static void draw_icon_piano2(int x, int y) { draw_icon_piano(x, y); }

static void draw_icon_graphing(int x, int y) {
    icon_bg_3d_round(x, y, 0x0F2942, 0x1A4973, 0x091B2E);
    bevel_box(x+4, y+4, 16, 16, 0x0D1F3C, 0x1E4D8C, 0x091B2E);
    gfx_draw_hline(x+6, y+14, 12, 0x1E3A5F);
    gfx_draw_hline(x+6, y+15, 12, 0x1E3A5F);
    int pts[7][2] = {{x+6,y+18},{x+8,y+14},{x+10,y+16},{x+12,y+12},{x+14,y+9},{x+16,y+10},{x+18,y+7}};
    for (int i = 0; i < 6; i++) {
        gfx_draw_line(pts[i][0], pts[i][1], pts[i+1][0], pts[i+1][1], 0x58A6FF);
        gfx_fill_circle(pts[i][0], pts[i][1], 1, 0x93C5FD);
    }
    gfx_fill_circle(pts[6][0], pts[6][1], 1, 0x93C5FD);
}

static void draw_icon_fallback(int x, int y) {
    icon_bg_3d_round(x, y, 0x334155, 0x4B5563, 0x1F2937);
    bevel_box(x+8, y+8, 8, 8, 0x475569, 0x64748B, 0x334155);
    gfx_draw_hline(x+8, y+12, 8, 0x94A3B8);
}

void draw_app_icon(const char* name, int x, int y) {
    if (!name) { draw_icon_fallback(x, y); return; }
    if (name[0] == 'H' && name[1] == 'T') { draw_icon_httpviewer(x, y); return; }
    if (name[0] == 'G' && name[6] == 'n') { draw_icon_graphing(x, y); return; }
    switch (name[0]) {
        case 'A':
            if (name[1] == 'c') draw_icon_accessibility(x, y);
            else if (name[1] == 'p') draw_icon_folder(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'B':
            if (name[1] == 'i') draw_icon_bitmap_maker(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'C':
            if (name[1] == 'a') {
                if (name[3] == 'e') draw_icon_calendar(x, y);
                else draw_icon_calc(x, y);
            } else if (name[1] == 'l') draw_icon_clock(x, y);
            else if (name[1] == 'u') draw_icon_colour(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'D':
            if (name[1] == 'e') {
                if (name[2] == 'm') draw_icon_shutdown(x, y);
                else if (name[2] == 'b') draw_icon_debug(x, y);
                else draw_icon_fallback(x, y);
            } else draw_icon_fallback(x, y);
            break;
        case 'F':
            if (name[1] == 'i') draw_icon_folder(x, y);
            else if (name[1] == 'l') draw_icon_flappy(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'G':
            if (name[1] == 'a') draw_icon_games(x, y);
            else if (name[1] == 'r') {
                if (name[6] == 'c') draw_icon_imgview(x, y);
                else draw_icon_fallback(x, y);
            } else draw_icon_fallback(x, y);
            break;
        case 'H': draw_icon_hexdump(x, y); break;
        case 'I': draw_icon_imgview(x, y); break;
        case 'K':
            if (name[1] == 'e') draw_icon_kernellog(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'M':
            if (name[1] == 'a') draw_icon_mandelbrot(x, y);
            else if (name[1] == 'i') draw_icon_mines(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'N': draw_icon_netdebug(x, y); break;
        case 'O': draw_icon_osk(x, y); break;
        case 'P':
            if (name[1] == 'C') draw_icon_pci(x, y);
            else if (name[1] == 'a') draw_icon_pairs(x, y);
            else if (name[1] == 'e') {
                if (name[3] == 'f') draw_icon_perfmon(x, y);
                else draw_icon_personalization(x, y);
            }
            else if (name[1] == 'i') draw_icon_piano(x, y);
            else if (name[1] == 'r') draw_icon_process(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'S':
            if (name[1] == 'n') draw_icon_snake(x, y);
            else if (name[1] == 'u') draw_icon_sudoku(x, y);
            else if (name[1] == 'y') draw_icon_system(x, y);
            else draw_icon_fallback(x, y);
            break;
        case 'T':
            if (name[1] == 'e') {
                if (name[2] == 'r') draw_icon_terminal(x, y);
                else if (name[2] == 't') draw_icon_tetris(x, y);
                else draw_icon_fallback(x, y);
            } else draw_icon_fallback(x, y);
            break;
        case '2': draw_icon_2048(x, y); break;
        default: draw_icon_fallback(x, y);
    }
}

void draw_start_icon(int x, int y, int w, int h) {
    personalization_t* p = get_personalization();
    uint32_t c = get_accent_color();
    uint32_t c_light = gfx_lighten(c, 40);
    uint32_t c_dark = gfx_darken(c, 30);
    int cx = x + w / 2;
    int cy = y + h / 2;
    gfx_draw_line(cx, cy - 10, cx + 10, cy, c_dark);
    gfx_draw_line(cx + 10, cy, cx, cy + 10, c_dark);
    gfx_draw_line(cx, cy + 10, cx - 10, cy, c_dark);
    gfx_draw_line(cx - 10, cy, cx, cy - 10, c_dark);
    gfx_draw_line(cx, cy - 8, cx + 8, cy, c);
    gfx_draw_line(cx + 8, cy, cx, cy + 8, c);
    gfx_draw_line(cx, cy + 8, cx - 8, cy, c);
    gfx_draw_line(cx - 8, cy, cx, cy - 8, c);
    gfx_draw_line(cx, cy - 6, cx + 2, cy - 2, c_light);
    gfx_draw_line(cx + 2, cy - 2, cx + 6, cy, c_light);
    gfx_draw_line(cx + 6, cy, cx + 2, cy + 2, c_light);
    gfx_draw_line(cx + 2, cy + 2, cx, cy + 6, c_light);
    gfx_draw_line(cx, cy + 6, cx - 2, cy + 2, c_light);
    gfx_draw_line(cx - 2, cy + 2, cx - 6, cy, c_light);
    gfx_draw_line(cx - 6, cy, cx - 2, cy - 2, c_light);
    gfx_draw_line(cx - 2, cy - 2, cx, cy - 6, c_light);
    bevel_circle(cx, cy, 2, c, c_light, c_dark);
    (void)p;
}

void draw_search_icon(int x, int y, int w, int h) {
    int cx = x + w / 2;
    int cy = y + h / 2;
    uint32_t c = 0x475569;
    gfx_draw_circle(cx - 2, cy - 2, 5, c);
    gfx_draw_circle(cx - 2, cy - 2, 4, c);
    gfx_draw_line(cx + 1, cy + 1, cx + 6, cy + 6, c);
    gfx_fill_circle(cx + 5, cy + 5, 2, c);
}

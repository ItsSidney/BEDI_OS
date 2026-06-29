#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"
#include "drivers/input/mouse.h"
#include "drivers/video/framebuffer.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

extern void colour_wheel_open_for_role(int role);

static int g_pers_win = -1;
static int g_pers_scroll_y = 0;

static int point_in(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

/* Palette: dark only */
static uint32_t bg_palette[] = {
    0x0A0C10, 0x0F1115, 0x141820, 0x181D25, 0x1A1D24, 0x1E222B,
    0x242830, 0x282D37, 0x2C303A, 0x323842, 0x383B44, 0x3F434D,
    0x454A55, 0x4D5059, 0x555A65, 0x5F6368
};
static uint32_t accent_palette[] = {
    0x3A86FF, 0x2D74E8, 0x1A5FCC, 0x669DF6, 0x8AB4F8, 0xADCCFB,
    0x7209B7, 0x8338EC, 0x9D4EDD, 0xAF5CF7, 0xC58AF9, 0xD4A5F5,
    0xFF006E, 0xFB5607, 0xFFBE0B, 0xFFE66D
};
static uint32_t font_palette[] = {
    0x000000, 0x0A0C10, 0x111827, 0x1F2937, 0x252830, 0x2C303A,
    0x374151, 0x434343, 0x4A4D56, 0x52575F, 0x5F6368, 0x6B7280,
    0x7C818A, 0x9CA3AF, 0xBDC1C6, 0xE5E7EB
};
static uint32_t button_palette[] = {
    0xF3F4F6, 0xE5E7EB, 0xD1D5DB, 0xCBD5E1, 0xB8BCC2, 0xA8AEB5,
    0xFFFFFF, 0xF9FAFB, 0xF3F4F6, 0xEFF6FF, 0xFDF2F8, 0xF2CC8C,
    0x8AB4F8, 0x669DF6, 0x3A86FF, 0x1A5FCC
};

static const char* bg_pattern_names[] = {
    "None", "Grid", "Dots", "Diamond", "Crosshatch", "Noise",
    "Waves", "Chequerboard", "Hexagons", "Bricks", "Triangles", "Lines"
};

static void apply_bg(void) {
    personalization_t* p = get_personalization();
    theme_set_custom_color(THEME_ROLE_BACKGROUND, bg_palette[p->bg_idx % 16]);
    theme_set_custom_color(THEME_ROLE_SURFACE, gfx_darken(bg_palette[p->bg_idx % 16], 10));
}

static void apply_accent(void) {
    personalization_t* p = get_personalization();
    theme_set_custom_color(THEME_ROLE_ACCENT, accent_palette[p->accent_idx % 16]);
}

static void apply_font(void) {
    personalization_t* p = get_personalization();
    theme_set_custom_color(THEME_ROLE_PRIMARY, font_palette[p->font_idx % 16]);
    theme_set_custom_color(THEME_ROLE_ON_PRIMARY, (p->font_idx < 8) ? 0xFFFFFF : 0x000000);
}

static void apply_buttons(void) {
    personalization_t* p = get_personalization();
    theme_set_custom_color(THEME_ROLE_BUTTON_BG, button_palette[p->btn_idx % 16]);
    theme_set_custom_color(THEME_ROLE_BUTTON_HOVER, gfx_darken(button_palette[p->btn_idx % 16], 10));
    theme_set_custom_color(THEME_ROLE_BUTTON_TEXT, (p->btn_idx < 8) ? 0x111827 : 0xFFFFFF);
    wm_refresh_all_button_theme();
}

static void apply_palette(void) {
    apply_bg();
    apply_accent();
    apply_font();
    apply_buttons();
}

static void draw_section_box(int x, int y, int w, const char* title, uint32_t accent, uint32_t border) {
    gfx_draw_rect_outline(x, y, w, 1, 1, border);
    gfx_draw_string_transparent(x + 10, y + 10, title, accent);
}

/* Backward-compatible wrapper for current callers */
static void draw_section_box_compat(int x, int y, int w, const char* title, uint32_t accent) {
    uint32_t border = theme_get_color(THEME_ROLE_OUTLINE);
    draw_section_box(x, y, w, title, accent, border);
}

static void draw_swatch_cell(int x, int y, int size, uint32_t color, int selected, uint32_t border) {
    gfx_fill_rect(x, y, size, size, color);
    gfx_draw_rect_outline(x, y, size, size, 1, 0x000000);
    if (selected) gfx_draw_rect_outline(x + 3, y + 3, size - 6, size - 6, 2, 0x8AB4F8);
}

static void personalization_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx;
    personalization_t* p = get_personalization();
    uint32_t win_bg = theme_get_color(THEME_ROLE_WINDOW_BG);
    uint32_t border = theme_get_color(THEME_ROLE_OUTLINE);
    uint32_t accent = theme_get_color(THEME_ROLE_ACCENT);
    gfx_fill_rect(x, y, w, h, win_bg);
    gfx_draw_rect_outline(x, y, w, h, 1, border);

    int cy = y + 16 - vy;
    int pad = 14;
    int lx = x + pad;
    int lw = w - pad * 2;

    gfx_draw_string_transparent(lx, cy, "PERSONALIZATION", accent);
    cy += 28;

    draw_section_box_compat(lx, cy, lw, "Background", accent);
    cy += 32;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int sx = lx + col * 28;
        int sy = cy + row * 22;
        uint32_t c = bg_palette[i];
        draw_swatch_cell(sx, sy, 22, c, p->bg_idx == i, border);
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, sx, sy, 22, 22)) {
            p->bg_idx = i;
            apply_bg();
        }
    }
    /* Custom colour button */
    {
        int bx = lx, by = cy + 2 * 22 + 6, bw = 80, bh = 22;
        uint32_t bg_btn = theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        gfx_fill_rect_rounded(bx, by, bw, bh, 4, bg_btn);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh, 4, 1, border);
        gfx_draw_string_transparent(bx + 6, by + 5, "Custom...", theme_get_color(THEME_ROLE_PRIMARY));
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, bx, by, bw, bh)) {
            colour_wheel_open_for_role(THEME_ROLE_BACKGROUND);
        }
    }
    cy += 3 * 22 + 12;

    gfx_draw_string_transparent(lx, cy, "Pattern:", theme_get_color(THEME_ROLE_PRIMARY));
    cy += 18;
    int bw = 120, bh = 28;
    for (int i = 0; i < 12; i++) {
        int bx = lx + (i % 4) * (bw + 8);
        int by = cy + (i / 4) * (bh + 6);
        uint32_t bg_btn = (p->bg_pattern == i) ? theme_get_color(THEME_ROLE_MENU_ITEM_SELECTED) : theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        gfx_fill_rect_rounded(bx, by, bw, bh, 6, bg_btn);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh, 6, 1, border);
        gfx_draw_string_transparent(bx + 6, by + 7, bg_pattern_names[i], theme_get_color(THEME_ROLE_PRIMARY));
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, bx, by, bw, bh)) {
            p->bg_pattern = i;
        }
    }
    cy += 3 * (bh + 6) + 12;

    /* Pattern size */
    {
        int sx = lx;
        int by = cy;
        int bw = 24, bh2 = 24;
        uint32_t btn_bg = theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        int mx = mouse_get_x(), my = mouse_get_y();
        int hover_minus = point_in(mx, my, sx, by, bw, bh2);
        if (hover_minus && (mouse_get_buttons() & 1)) {
            if (p->bg_pattern_size > 1) p->bg_pattern_size /= 2;
        }
        gfx_fill_rect_rounded(sx, by, bw, bh2, 4, hover_minus ? gfx_lighten(btn_bg, 10) : btn_bg);
        gfx_draw_rect_rounded_outline(sx, by, bw, bh2, 4, 1, border);
        gfx_draw_string_transparent(sx + 8, by + 6, "-", theme_get_color(THEME_ROLE_PRIMARY));

        int tx = sx + bw + 8;
        char size_buf[4]; size_buf[0] = '0' + p->bg_pattern_size; size_buf[1] = 0;
        gfx_draw_string_transparent(tx, by + 6, size_buf, theme_get_color(THEME_ROLE_PRIMARY));

        int bx = tx + 20;
        int hover_plus = point_in(mx, my, bx, by, bw, bh2);
        if (hover_plus && (mouse_get_buttons() & 1)) {
            if (p->bg_pattern_size < 8) p->bg_pattern_size *= 2;
        }
        gfx_fill_rect_rounded(bx, by, bw, bh2, 4, hover_plus ? gfx_lighten(btn_bg, 10) : btn_bg);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh2, 4, 1, border);
        gfx_draw_string_transparent(bx + 8, by + 6, "+", theme_get_color(THEME_ROLE_PRIMARY));

        cy += 34;
    }

    draw_section_box_compat(lx, cy, lw, "Accent", accent);
    cy += 32;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int sx = lx + col * 28;
        int sy = cy + row * 22;
        uint32_t c = accent_palette[i];
        draw_swatch_cell(sx, sy, 22, c, p->accent_idx == i, border);
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, sx, sy, 22, 22)) {
            p->accent_idx = i;
            apply_accent();
        }
    }
    /* Custom colour button */
    {
        int bx = lx, by = cy + 2 * 22 + 6, bw = 80, bh = 22;
        uint32_t bg_btn = theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        gfx_fill_rect_rounded(bx, by, bw, bh, 4, bg_btn);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh, 4, 1, border);
        gfx_draw_string_transparent(bx + 6, by + 5, "Custom...", theme_get_color(THEME_ROLE_PRIMARY));
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, bx, by, bw, bh)) {
            colour_wheel_open_for_role(THEME_ROLE_ACCENT);
        }
    }
    cy += 3 * 22 + 12;

    draw_section_box_compat(lx, cy, lw, "Font color", accent);
    cy += 32;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int sx = lx + col * 28;
        int sy = cy + row * 22;
        uint32_t c = font_palette[i];
        draw_swatch_cell(sx, sy, 22, c, p->font_idx == i, border);
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, sx, sy, 22, 22)) {
            p->font_idx = i;
            apply_font();
        }
    }
    /* Custom colour button */
    {
        int bx = lx, by = cy + 2 * 22 + 6, bw = 80, bh = 22;
        uint32_t bg_btn = theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        gfx_fill_rect_rounded(bx, by, bw, bh, 4, bg_btn);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh, 4, 1, border);
        gfx_draw_string_transparent(bx + 6, by + 5, "Custom...", theme_get_color(THEME_ROLE_PRIMARY));
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, bx, by, bw, bh)) {
            colour_wheel_open_for_role(THEME_ROLE_PRIMARY);
        }
    }
    cy += 3 * 22 + 12;

    draw_section_box_compat(lx, cy, lw, "Button color", accent);
    cy += 32;
    for (int i = 0; i < 16; i++) {
        int col = i % 8;
        int row = i / 8;
        int sx = lx + col * 28;
        int sy = cy + row * 22;
        uint32_t c = button_palette[i];
        draw_swatch_cell(sx, sy, 22, c, p->btn_idx == i, border);
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, sx, sy, 22, 22)) {
            p->btn_idx = i;
            apply_buttons();
        }
    }
    /* Custom colour button */
    {
        int bx = lx, by = cy + 2 * 22 + 6, bw = 80, bh = 22;
        uint32_t bg_btn = theme_get_color(THEME_ROLE_SURFACE_VARIANT);
        gfx_fill_rect_rounded(bx, by, bw, bh, 4, bg_btn);
        gfx_draw_rect_rounded_outline(bx, by, bw, bh, 4, 1, border);
        gfx_draw_string_transparent(bx + 6, by + 5, "Custom...", theme_get_color(THEME_ROLE_PRIMARY));
        int mx = mouse_get_x(), my = mouse_get_y();
        if ((mouse_get_buttons() & 1) && point_in(mx, my, bx, by, bw, bh)) {
            colour_wheel_open_for_role(THEME_ROLE_BUTTON_BG);
        }
    }
}

void personalization_app(void) {
    if (g_pers_win >= 0) {
        wm_close_window(g_pers_win);
        g_pers_win = -1;
        g_pers_scroll_y = 0;
        return;
    }
    int w = 760, h = 640;
    int x = (get_fb_width() - w) / 2;
    int y = (get_fb_height() - h) / 2;
    g_pers_win = wm_open_window(x, y, w, h, "Personalization", theme_get_color(THEME_ROLE_ACCENT),
                                personalization_render, NULL, NULL);
    if (g_pers_win >= 0) {
        wm_window_t* win = wm_get_window(g_pers_win);
        if (win) win->content_h = 660;
    }
}

#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include <stdint.h>
#include <stdio.h>
#include <math.h>

static float fmodf(float x, float y) { return __builtin_fmodf(x, y); }

#define CW_W 420
#define CW_H 280
#define CW_PREVIEW_W 200
#define CW_PREVIEW_H 36
#define CW_PREVIEW_X 110
#define CW_PREVIEW_Y 16
#define CW_SLIDER_X 40
#define CW_SLIDER_W 340
#define CW_SLIDER_H 18
#define CW_SLIDER_GAP 30
#define CW_SLIDER_LABEL_Y_OFFSET 14

static int cw_win = -1;
static float cw_hue = 0.0f;
static float cw_sat = 1.0f;
static float cw_val = 1.0f;
static int cw_dragging = 0;
static int cw_role = -1;
static int cw_apply = 0;

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px <= rx + rw && py >= ry && py <= ry + rh;
}

static uint32_t hsv_to_rgb(float h, float s, float v) {
    if (s <= 0.0f) {
        int vi = (int)(v * 255); if (vi > 255) vi = 255;
        return (vi << 16) | (vi << 8) | vi;
    }
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    float hh = h / 60.0f;
    int i = (int)hh;
    float f = hh - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - f * s);
    float t = v * (1.0f - (1.0f - f) * s);
    float r=0, g=0, b=0;
    switch (i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        case 5: r=v; g=p; b=q; break;
    }
    int ri = (int)(r * 255); if (ri > 255) ri = 255;
    int gi = (int)(g * 255); if (gi > 255) gi = 255;
    int bi = (int)(b * 255); if (bi > 255) bi = 255;
    return (ri << 16) | (gi << 8) | bi;
}

static int cw_hit_slider(int mx, int my, int wx, int wy, int sy) {
    return mx >= wx + 8 && mx <= wx + 8 + CW_SLIDER_W && my >= wy + sy && my <= wy + sy + CW_SLIDER_H;
}

static void apply_cw_color(void) {
    if (cw_role < 0) return;
    uint32_t sel = hsv_to_rgb(cw_hue, cw_sat, cw_val);
    theme_set_custom_color((theme_role_t)cw_role, sel);
    if (cw_role == THEME_ROLE_BACKGROUND) theme_set_custom_color(THEME_ROLE_SURFACE, sel);
    if (cw_role == THEME_ROLE_PRIMARY) theme_set_custom_color(THEME_ROLE_ON_PRIMARY, 0x000000);
    if (cw_role == THEME_ROLE_BUTTON_BG) {
        theme_set_custom_color(THEME_ROLE_BUTTON_HOVER, sel);
        theme_set_custom_color(THEME_ROLE_BUTTON_TEXT, 0xFFFFFF);
        wm_refresh_all_button_theme();
    }
}

static void colour_wheel_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)w; (void)h; (void)vx; (void)vy;
    static int prev_mbtn = 0;
    int ax = x;
    int ay = y;
    uint32_t bg = 0x252830;
    uint32_t border = 0x2C303A;
    uint32_t text = 0xE4E6EA;
    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int left_clk = (mbtn & MOUSE_LEFT) && !(prev_mbtn & MOUSE_LEFT);

    gfx_fill_rect(ax, ay, CW_W, CW_H, bg);
    gfx_draw_rect_outline(ax, ay, CW_W, CW_H, 1, border);

    /* Preview */
    uint32_t sel = hsv_to_rgb(cw_hue, cw_sat, cw_val);
    gfx_fill_rect(ax + CW_PREVIEW_X, ay + CW_PREVIEW_Y, CW_PREVIEW_W, CW_PREVIEW_H, sel);
    gfx_draw_rect_outline(ax + CW_PREVIEW_X, ay + CW_PREVIEW_Y, CW_PREVIEW_W, CW_PREVIEW_H, 1, 0xFFFFFF);
    int ri = (sel >> 16) & 0xFF;
    int gi = (sel >> 8)  & 0xFF;
    int bi = sel & 0xFF;
    char buf[16];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", ri, gi, bi);
    gfx_draw_string_transparent(ax + CW_PREVIEW_X + 8, ay + CW_PREVIEW_Y + CW_PREVIEW_H + 8, buf, text);

    /* Sliders */
    int sy = ay + CW_PREVIEW_Y + CW_PREVIEW_H + 18;
    int sx = ax + CW_SLIDER_X;
    int sw = CW_SLIDER_W;
    int sh = CW_SLIDER_H;

    gfx_draw_string_transparent(sx, sy - CW_SLIDER_LABEL_Y_OFFSET, "Hue", text);
    gfx_fill_rect(sx, sy, sw, sh, 0x333333);
    for (int i = 0; i < sw; i++) {
        float hh = (float)i * 360.0f / (float)sw;
        gfx_fill_rect(sx + i, sy, 1, sh, hsv_to_rgb(hh, 1.0f, 1.0f));
    }
    int hx = sx + (int)(cw_hue * (float)sw / 360.0f);
    gfx_draw_rect_outline(hx - 3, sy - 2, 6, sh + 4, 1, 0xFFFFFF);

    sy += sh + CW_SLIDER_GAP;
    gfx_draw_string_transparent(sx, sy - CW_SLIDER_LABEL_Y_OFFSET, "Saturation", text);
    gfx_fill_gradient_h(sx, sy, sw, sh, 0xFFFFFF, hsv_to_rgb(cw_hue, 1.0f, 1.0f));
    int sx_pos = sx + (int)(cw_sat * (float)sw);
    gfx_draw_rect_outline(sx_pos - 3, sy - 2, 6, sh + 4, 1, 0xFFFFFF);

    sy += sh + CW_SLIDER_GAP;
    gfx_draw_string_transparent(sx, sy - CW_SLIDER_LABEL_Y_OFFSET, "Value", text);
    gfx_fill_gradient_h(sx, sy, sw, sh, 0x000000, hsv_to_rgb(cw_hue, cw_sat, 1.0f));
    int vx_pos = sx + (int)(cw_val * (float)sw);
    gfx_draw_rect_outline(vx_pos - 3, sy - 2, 6, sh + 4, 1, 0xFFFFFF);

    /* Apply/Cancel */
    int by = ay + 210;
    int bx1 = ax + 60;
    int bx2 = ax + 200;
    uint32_t btn_bg = 0x2C303A;
    gfx_fill_rect_rounded(bx1, by, 90, 28, 6, btn_bg);
    gfx_draw_rect_rounded_outline(bx1, by, 90, 28, 6, 1, border);
    gfx_draw_string_transparent(bx1 + 16, by + 8, "Apply", text);

    gfx_fill_rect_rounded(bx2, by, 90, 28, 6, btn_bg);
    gfx_draw_rect_rounded_outline(bx2, by, 90, 28, 6, 1, border);
    gfx_draw_string_transparent(bx2 + 14, by + 8, "Cancel", text);

    if (left_clk) {
        if (point_in_rect(mx, my, bx1, by, 90, 28)) {
            cw_apply = 1;
            apply_cw_color();
            wm_close_window(cw_win);
            cw_win = -1;
            cw_role = -1;
            return;
        }
        if (point_in_rect(mx, my, bx2, by, 90, 28)) {
            wm_close_window(cw_win);
            cw_win = -1;
            cw_role = -1;
            return;
        }
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18)) {
            cw_hue = ((float)(mx - (ax + CW_SLIDER_X + 8)) * 360.0f) / (float)CW_SLIDER_W;
            if (cw_hue < 0) cw_hue = 0;
            if (cw_hue > 360) cw_hue = 360;
        }
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18 + CW_SLIDER_H + CW_SLIDER_GAP)) {
            cw_sat = (float)(mx - (ax + CW_SLIDER_X + 8)) / (float)CW_SLIDER_W;
            if (cw_sat < 0) cw_sat = 0;
            if (cw_sat > 1) cw_sat = 1;
        }
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18 + (CW_SLIDER_H + CW_SLIDER_GAP) * 2)) {
            cw_val = (float)(mx - (ax + CW_SLIDER_X + 8)) / (float)CW_SLIDER_W;
            if (cw_val < 0) cw_val = 0;
            if (cw_val > 1) cw_val = 1;
        }
    }

    /* Drag sliders */
    if ((mbtn & MOUSE_LEFT)) {
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18)) {
            cw_hue = ((float)(mx - (ax + CW_SLIDER_X + 8)) * 360.0f) / (float)CW_SLIDER_W;
            if (cw_hue < 0) cw_hue = 0;
            if (cw_hue > 360) cw_hue = 360;
        }
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18 + CW_SLIDER_H + CW_SLIDER_GAP)) {
            cw_sat = (float)(mx - (ax + CW_SLIDER_X + 8)) / (float)CW_SLIDER_W;
            if (cw_sat < 0) cw_sat = 0;
            if (cw_sat > 1) cw_sat = 1;
        }
        if (cw_hit_slider(mx, my, ax, ay, CW_PREVIEW_Y + CW_PREVIEW_H + 18 + (CW_SLIDER_H + CW_SLIDER_GAP) * 2)) {
            cw_val = (float)(mx - (ax + CW_SLIDER_X + 8)) / (float)CW_SLIDER_W;
            if (cw_val < 0) cw_val = 0;
            if (cw_val > 1) cw_val = 1;
        }
    }

    prev_mbtn = mbtn;
}

void colour_wheel_app(void) {
    if (cw_win >= 0) {
        wm_close_window(cw_win);
        cw_win = -1;
        return;
    }
    int w = CW_W, h = CW_H;
    int sx = (get_fb_width() - w) / 2;
    int sy = (get_fb_height() - h) / 2;
    cw_win = wm_open_window(sx, sy, w, h, "Colour", 0x252830,
                            colour_wheel_render, NULL, NULL);
}

void colour_wheel_open_for_role(int role) {
    cw_role = role;
    if (cw_win >= 0) {
        wm_bring_to_front(cw_win);
    } else {
        colour_wheel_app();
    }
}

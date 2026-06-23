#include "gui/ui.h"
#include "drivers/video/gfx.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "drivers/video/framebuffer.h"
#include "gui/gui.h"

void ui_init_context(ui_context_t* ctx, int x, int y, int w, int h, const char* title, uint32_t accent, int is_square) {
    ctx->x = x; ctx->y = y; ctx->w = w; ctx->h = h;
    for (int i = 0; i < 63; i++) {
        if (!title[i]) { ctx->title[i] = 0; break; }
        ctx->title[i] = title[i];
    }
    ctx->title[63] = 0;
    ctx->accent_color = accent;
    ctx->is_square = is_square;
    ctx->button_count = 0;
    ctx->on_render = 0;
    ctx->on_key = 0;
    ctx->is_running = 1;
}

void ui_add_button(ui_context_t* ctx, int id, int x, int y, int w, int h, const char* label, uint32_t bg, uint32_t fg, ui_click_cb cb) {
    if (ctx->button_count >= UI_MAX_BUTTONS) return;
    ui_button_t* btn = &ctx->buttons[ctx->button_count++];
    btn->id = id; btn->x = x; btn->y = y; btn->w = w; btn->h = h;
    btn->bg_color = bg; btn->fg_color = fg; btn->on_click = cb;
    btn->is_hovered = 0; btn->is_active = 1;
    for (int i = 0; i < 31; i++) {
        if (!label[i]) { btn->label[i] = 0; break; }
        btn->label[i] = label[i];
    }
    btn->label[31] = 0;
}

void ui_set_button_active(ui_context_t* ctx, int id, int active) {
    for (int i = 0; i < ctx->button_count; i++) {
        if (ctx->buttons[i].id == id) { ctx->buttons[i].is_active = active; return; }
    }
}

static int ui_hit_test(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px <= x + w && py >= y && py <= y + h);
}

void ui_app_run(ui_context_t* ctx) {
    static int last_mbtns = 0;
    while (ctx->is_running) {
        int mx = mouse_get_x(), my = mouse_get_y();
        int mbtns = mouse_get_buttons();
        int clicked = (mbtns & 1) && !(last_mbtns & 1);
        char key = get_key();

        if (clicked && ui_hit_test(mx, my, ctx->x + 14 - 6, ctx->y + 14 - 6, 12, 12)) {
            ctx->is_running = 0;
        }

        for (int b = 0; b < ctx->button_count; b++) {
            ui_button_t* btn = &ctx->buttons[b];
            if (!btn->is_active) { btn->is_hovered = 0; continue; }
            int abs_x = ctx->x + btn->x;
            int abs_y = ctx->y + 28 + btn->y;
            btn->is_hovered = ui_hit_test(mx, my, abs_x, abs_y, btn->w, btn->h);
            if (clicked && btn->is_hovered && btn->on_click) {
                btn->on_click(btn->id);
            }
        }

        if (key && ctx->on_key) ctx->on_key(key);
        if (key == KEY_ESC) ctx->is_running = 0;

        draw_premium_wallpaper();
        gfx_draw_window_custom(ctx->x, ctx->y, ctx->w, ctx->h, ctx->title, ctx->accent_color, ctx->is_square);
        if (ctx->on_render) ctx->on_render(ctx->x, ctx->y + 28, ctx->w, ctx->h - 28);
        for (int b = 0; b < ctx->button_count; b++) {
            ui_button_t* btn = &ctx->buttons[b];
            if (!btn->is_active) continue;
            if (btn->bg_color == 0 && btn->label[0] == 0) continue;
            gfx_draw_button(ctx->x + btn->x, ctx->y + 28 + btn->y, btn->w, btn->h, btn->label, btn->bg_color, btn->fg_color, btn->is_hovered);
        }
        draw_taskbar();
        mouse_draw_cursor();
        swap_buffers();
        last_mbtns = mbtns;
        for (volatile int i=0; i<30000; i++);
    }
}

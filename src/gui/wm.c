#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/mouse.h"
#include "drivers/input/keyboard.h"
#include "kernel/time/timer.h"
#include <stddef.h>

extern void draw_premium_wallpaper(void);
extern void draw_taskbar(void);

#define MAX_BUTTONS_PER_WINDOW 512

static wm_window_t windows[WM_MAX_WINDOWS];
static int window_count = 0;
static int focused_id = -1;
static int next_win_id = 1;
static int prev_mouse_btn = 0;

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px <= rx + rw && py >= ry && py <= ry + rh);
}

void wm_init(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        windows[i].id = -1;
        windows[i].flags = 0;
        windows[i].button_count = 0;
    }
}

static wm_window_t* find_window(int id) {
    if (id < 0) return 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id == id) return &windows[i];
    }
    return 0;
}

static void bring_to_front(wm_window_t* win) {
    if (!win) return;
    int max_z = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id != -1 && (windows[i].flags & WM_FLAG_VISIBLE)) {
            if (windows[i].z_order > max_z) max_z = windows[i].z_order;
            windows[i].flags &= ~WM_FLAG_FOCUSED;
        }
    }
    win->z_order = max_z + 1;
    win->flags |= WM_FLAG_FOCUSED;
    focused_id = win->id;
}

int wm_open_window(int x, int y, int w, int h, const char* title, uint32_t accent,
                   wm_render_cb on_render,
                   wm_key_cb on_key,
                   wm_resize_cb on_resize) {
    int slot = -1;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id == -1) { slot = i; break; }
    }
    if (slot == -1) return -1;

    wm_window_t* win = &windows[slot];
    win->id = next_win_id++;
    win->x = x; win->y = y; win->w = w; win->h = h;
    win->view_x = 0; win->view_y = 0;
    win->content_w = w; win->content_h = h - WM_TITLEBAR_H;
    int j = 0; while (title[j] && j < 63) { win->title[j] = title[j]; j++; } win->title[j] = 0;
    win->accent_color = accent;
    win->flags = WM_FLAG_VISIBLE;
    win->on_render = on_render;
    win->on_key = on_key;
    win->on_resize = on_resize;
    win->button_count = 0;
    win->app_data = 0;

    bring_to_front(win);
    window_count++;
    return win->id;
}

void wm_close_window(int id) {
    wm_window_t* win = find_window(id);
    if (win) {
        win->id = -1;
        win->flags = 0;
        window_count--;
        if (focused_id == id) focused_id = -1;
    }
}

void wm_add_button(int win_id, int btn_id, int x, int y, int w, int h, const char* label,
                  uint32_t bg, uint32_t fg, void (*on_click)(int, int)) {
    wm_window_t* win = find_window(win_id);
    if (!win || win->button_count >= MAX_BUTTONS_PER_WINDOW) return;
    int b = win->button_count++;
    win->buttons[b].id = btn_id;
    win->buttons[b].x = x; win->buttons[b].y = y;
    win->buttons[b].w = w; win->buttons[b].h = h;
    int j = 0; while (label[j] && j < 31) { win->buttons[b].label[j] = label[j]; j++; } win->buttons[b].label[j] = 0;
    win->buttons[b].bg_color = bg;
    win->buttons[b].fg_color = fg;
    win->buttons[b].on_click = on_click;
    win->buttons[b].is_hovered = 0;
    win->buttons[b].is_active = 1;
}

void wm_clear_buttons(int win_id) {
    wm_window_t* win = find_window(win_id);
    if (win) win->button_count = 0;
}

void wm_set_button_active(int win_id, int btn_id, int active) {
    wm_window_t* win = find_window(win_id);
    if (!win) return;
    for (int i = 0; i < win->button_count; i++) {
        if (win->buttons[i].id == btn_id) { win->buttons[i].is_active = active; break; }
    }
}

static void get_sorted_windows(wm_window_t** sorted, int* count) {
    *count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (windows[i].id != -1 && (windows[i].flags & WM_FLAG_VISIBLE)) {
            sorted[(*count)++] = &windows[i];
        }
    }
    for (int i = 0; i < *count - 1; i++) {
        for (int j = 0; j < *count - i - 1; j++) {
            if (sorted[j]->z_order > sorted[j+1]->z_order) {
                wm_window_t* tmp = sorted[j]; sorted[j] = sorted[j+1]; sorted[j+1] = tmp;
            }
        }
    }
}

static wm_window_t* window_at_point(int px, int py) {
    wm_window_t* sorted[WM_MAX_WINDOWS];
    int count;
    get_sorted_windows(sorted, &count);
    for (int i = count - 1; i >= 0; i--) {
        if (point_in_rect(px, py, sorted[i]->x, sorted[i]->y, sorted[i]->w, sorted[i]->h)) return sorted[i];
    }
    return 0;
}

static void render_window(wm_window_t* win) {
    int x = win->x, y = win->y, w = win->w, h = win->h;
    int is_focused = (win->flags & WM_FLAG_FOCUSED) != 0;

    gfx_push_clip(x, y, w, h);

    personalization_t* p = get_personalization();
    uint32_t bg_main = (p->theme == 0) ? 0x15171D : 0xF8F9FA;
    uint32_t text_clr = (p->theme == 0) ? 0xE4E6EA : 0x202124;
    uint32_t border_clr = is_focused ? (p->theme == 0 ? 0x383B44 : 0xDADCE0) : (p->theme == 0 ? 0x262830 : 0xE8EAED);
    uint32_t title_bg = (p->theme == 0) ? 0x1D1F26 : 0xF0F1F3;
    uint32_t accent = win->accent_color;

    if (is_focused) {
        gfx_fill_rect_alpha(x + 3, y + h, w - 4, 3, 0x000000, 25);
    }

    gfx_fill_rect(x + 1, y + WM_TITLEBAR_H, w - 2, h - WM_TITLEBAR_H, bg_main);
    gfx_draw_rect_outline(x, y, w + 1, h + 1, 1, border_clr);

    int title_h = WM_TITLEBAR_H;
    gfx_fill_rect(x + 1, y + 1, w - 2, title_h - 1, title_bg);

    if (is_focused) {
        gfx_fill_rect(x + 1, y + title_h, w - 2, 1, gfx_darken(border_clr, 10));
    } else {
        gfx_draw_hline(x + 1, y + title_h, w - 2, gfx_darken(border_clr, 15));
    }

    gfx_draw_string_transparent(x + 12, y + (title_h - 16) / 2, win->title, text_clr);

    int btn_size = title_h - 10;
    int btn_y = y + 5;
    int close_x = x + w - btn_size - 6;
    win->close_btn_x = close_x - 4;
    win->close_btn_y = y + 2;
    win->close_btn_w = btn_size + 8;
    win->close_btn_h = title_h - 4;

    int mx = mouse_get_x(), my = mouse_get_y();
    int close_hover = point_in_rect(mx, my, win->close_btn_x, win->close_btn_y, win->close_btn_w, win->close_btn_h);
    gfx_fill_rect(close_x, btn_y, btn_size, btn_size, close_hover ? 0xE81123 : (p->theme == 0 ? 0x262830 : 0xDADCE0));
    gfx_draw_string_transparent(close_x + 5, btn_y + 2, "x", text_clr);

    gfx_push_clip(x + 1, y + WM_TITLEBAR_H + 1, w - 2, h - WM_TITLEBAR_H - 2);
    if (win->on_render) {
        win->on_render(win->id, x, y + WM_TITLEBAR_H, w, h - WM_TITLEBAR_H, win->view_x, win->view_y);
    }
    gfx_pop_clip();

    for (int b = 0; b < win->button_count; b++) {
        if (!win->buttons[b].is_active) continue;
        int bx = x + win->buttons[b].x;
        int by = y + WM_TITLEBAR_H + win->buttons[b].y;
        uint32_t btn_bg = win->buttons[b].is_hovered ? gfx_lighten(win->buttons[b].bg_color, 20) : win->buttons[b].bg_color;
        gfx_fill_rect_rounded(bx, by, win->buttons[b].w, win->buttons[b].h, 4, btn_bg);
        gfx_draw_string_transparent(bx + (win->buttons[b].w - gfx_strlen(win->buttons[b].label) * 8) / 2,
                                    by + (win->buttons[b].h - 16) / 2, win->buttons[b].label, win->buttons[b].fg_color);
    }

    if (win->content_h > (h - WM_TITLEBAR_H)) {
        int sb_w = 6, sb_x = x + w - sb_w - 2, sb_y = y + WM_TITLEBAR_H + 2, sb_h = h - WM_TITLEBAR_H - 4;
        gfx_fill_rect(sb_x, sb_y, sb_w, sb_h, (p->theme == 0 ? 0x0D0E12 : 0xF0F1F3));
        int thumb_h = (sb_h * sb_h) / win->content_h;
        if (thumb_h < 16) thumb_h = 16;
        int thumb_y = sb_y + (win->view_y * (sb_h - thumb_h)) / (win->content_h - (h - WM_TITLEBAR_H));
        gfx_fill_rect(sb_x + 1, thumb_y, sb_w - 2, thumb_h, border_clr);
    }

    uint32_t handle_clr = (p->theme == 0) ? 0x4D5059 : 0xBDC1C6;
    for (int i = 0; i < 3; i++) {
        uint32_t c = gfx_lerp_color(handle_clr, bg_main, i, 3);
        gfx_fill_rect(x + w - 8 - i*4, y + h - 4, 3, 3, c);
    }

    gfx_pop_clip();
}

static void draw_resize_grid(int x, int y, int w, int h, uint32_t color) {
    int cl = 16;
    gfx_draw_line(x, y, x + cl, y, color);
    gfx_draw_line(x, y, x, y + cl, color);
    gfx_draw_line(x + w, y, x + w - cl, y, color);
    gfx_draw_line(x + w, y, x + w, y + cl, color);
    gfx_draw_line(x, y + h, x + cl, y + h, color);
    gfx_draw_line(x, y + h, x, y + h - cl, color);
    gfx_draw_line(x + w, y + h, x + w - cl, y + h, color);
    gfx_draw_line(x + w, y + h, x + w, y + h - cl, color);
    gfx_draw_rect_outline(x, y, w, h, 1, color);
}

int wm_tick(void) {
    int mx = mouse_get_x(), my = mouse_get_y(), mbtn = mouse_get_buttons();
    int clicked = (mbtn & 1) && !(prev_mouse_btn & 1);
    unsigned char key = (unsigned char)get_key();
    uint32_t fw = get_fb_width(), fh = get_fb_height();

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t* win = &windows[i];
        if (!(win->flags & WM_FLAG_VISIBLE)) continue;
        if (win->flags & WM_FLAG_DRAGGING) {
            if (mbtn & 1) {
                win->x = mx - win->drag_offset_x; win->y = my - win->drag_offset_y;
                if (win->y < 0) win->y = 0;
            } else win->flags &= ~WM_FLAG_DRAGGING;
        }
        if (win->flags & WM_FLAG_RESIZING) {
            if (mbtn & 1) {
                int dw = mx - win->drag_offset_x;
                int dh = my - win->drag_offset_y;
                win->w = win->start_w + dw;
                win->h = win->start_h + dh;
                if (win->w < WM_MIN_W) win->w = WM_MIN_W;
                if (win->h < WM_MIN_H) win->h = WM_MIN_H;
                win->content_w = win->w;
                win->content_h = win->h - WM_TITLEBAR_H;
                if (win->on_resize) win->on_resize(win->id, win->w, win->h);
            } else win->flags &= ~WM_FLAG_RESIZING;
        }
    }

    if (clicked) {
        wm_window_t* hit = window_at_point(mx, my);
        if (hit) {
            bring_to_front(hit);

            if (point_in_rect(mx, my, hit->close_btn_x, hit->close_btn_y, hit->close_btn_w, hit->close_btn_h)) {
                wm_close_window(hit->id); goto done;
            }

            if (point_in_rect(mx, my, hit->x + hit->w - 20, hit->y + hit->h - 20, 20, 20)) {
                hit->flags |= WM_FLAG_RESIZING;
                hit->start_w = hit->w; hit->start_h = hit->h;
                hit->drag_offset_x = mx; hit->drag_offset_y = my;
                goto done;
            }

            if (point_in_rect(mx, my, hit->x, hit->y, hit->w, WM_TITLEBAR_H)) {
                hit->flags |= WM_FLAG_DRAGGING; hit->drag_offset_x = mx - hit->x; hit->drag_offset_y = my - hit->y;
            }

            if (hit->content_h > (hit->h - WM_TITLEBAR_H)) {
                int sb_w = 10, sb_x = hit->x + hit->w - sb_w - 2;
                if (point_in_rect(mx, my, sb_x, hit->y + WM_TITLEBAR_H, sb_w, hit->h - WM_TITLEBAR_H)) {
                    hit->view_y = ((my - (hit->y + WM_TITLEBAR_H)) * hit->content_h) / (hit->h - WM_TITLEBAR_H) - (hit->h / 4);
                    if (hit->view_y < 0) hit->view_y = 0;
                    int max_v = hit->content_h - (hit->h - WM_TITLEBAR_H);
                    if (hit->view_y > max_v) hit->view_y = (max_v > 0 ? max_v : 0);
                    goto done;
                }
            }

            for (int b = 0; b < hit->button_count; b++) {
                if (!hit->buttons[b].is_active) continue;
                int bx = hit->x + hit->buttons[b].x;
                int by = hit->y + WM_TITLEBAR_H + hit->buttons[b].y;
                if (point_in_rect(mx, my, bx, by, hit->buttons[b].w, hit->buttons[b].h)) {
                    if (hit->buttons[b].on_click) hit->buttons[b].on_click(hit->id, hit->buttons[b].id);
                    goto done;
                }
            }
        } else if (my < (int)fh - 36) {
            focused_id = -1; for (int i = 0; i < WM_MAX_WINDOWS; i++) windows[i].flags &= ~WM_FLAG_FOCUSED;
        }
    }
done:
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_window_t* win = &windows[i];
        if (!(win->flags & WM_FLAG_VISIBLE)) continue;
        for (int b = 0; b < win->button_count; b++) {
            win->buttons[b].is_hovered = win->buttons[b].is_active && point_in_rect(mx, my, win->x + win->buttons[b].x, win->y + WM_TITLEBAR_H + win->buttons[b].y, win->buttons[b].w, win->buttons[b].h);
        }
    }

    extern void gui_toggle_start_menu(void), gui_open_search(void), gui_handle_menu_key(char), gui_handle_search_key(char);
    extern int gui_is_menu_open(void), gui_is_search_open(void);
    if ((keyboard_is_key_down(0x5B) || keyboard_is_key_down(0x5C)) && (key == 's' || key == 'S')) { gui_open_search(); key = 0; }
    else if (key == 132) { gui_toggle_start_menu(); key = 0; }
    if (key != 0) {
        if (gui_is_search_open()) { gui_handle_search_key(key); key = 0; }
        else if (gui_is_menu_open()) { gui_handle_menu_key(key); key = 0; }

        if (focused_id >= 0) {
            wm_window_t* fw = find_window(focused_id);
            int has_scroll = fw && (fw->content_h > (fw->h - WM_TITLEBAR_H));
            if (has_scroll && (keyboard_is_key_down(0x2A) || keyboard_is_key_down(0x36))) {
                if (KEY_MATCH(key, KEY_UP)) { fw->view_y -= 40; key = 0; }
                else if (KEY_MATCH(key, KEY_DOWN)) { fw->view_y += 40; key = 0; }
                if (fw->view_y < 0) fw->view_y = 0;
                int max_v = fw->content_h - (fw->h - WM_TITLEBAR_H);
                if (fw->view_y > max_v) fw->view_y = (max_v > 0 ? max_v : 0);
            }
        }
    }

    if (key && focused_id >= 0) { wm_window_t* fw = find_window(focused_id); if (fw && fw->on_key) fw->on_key(fw->id, key); }

    wm_window_t* sorted[WM_MAX_WINDOWS]; int count; get_sorted_windows(sorted, &count);
    for (int i = 0; i < count; i++) {
        render_window(sorted[i]);
        if (sorted[i]->flags & WM_FLAG_RESIZING) {
            draw_resize_grid(sorted[i]->x, sorted[i]->y, sorted[i]->w, sorted[i]->h,
                           (get_personalization()->theme == 0) ? 0x6B7280 : 0x9CA3AF);
        }
    }
    prev_mouse_btn = mbtn;
    return 0;
}

void wm_run_single(int win_id) {
    wm_window_t* win = find_window(win_id); if (!win) return;
    while (win->flags & WM_FLAG_VISIBLE) { draw_premium_wallpaper(); wm_tick(); draw_taskbar(); mouse_draw_cursor(); swap_buffers(); sleep_ms(16); }
}

wm_window_t* wm_get_window_by_index(int index) {
    if (index < 0 || index >= WM_MAX_WINDOWS || !(windows[index].flags & WM_FLAG_VISIBLE)) return 0;
    return &windows[index];
}

void wm_bring_to_front(int win_id) { wm_window_t* win = find_window(win_id); if (win) bring_to_front(win); }
int wm_get_focused(void) { return focused_id; }
wm_window_t* wm_get_window(int id) { return find_window(id); }
int wm_get_window_count(void) { return window_count; }
void wm_set_app_data(int win_id, void* data) { wm_window_t* win = find_window(win_id); if (win) win->app_data = data; }
void* wm_get_app_data(int win_id) { wm_window_t* win = find_window(win_id); return win ? win->app_data : 0; }

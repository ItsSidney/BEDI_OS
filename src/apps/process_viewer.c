#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "kernel/task/task.h"
#include "kernel/time/timer.h"
#include "drivers/video/framebuffer.h"

static int pv_win_id = -1;
static int pv_scroll = 0;
static int pv_selected = 0;
static int pv_kill_mode = 0;

static void pv_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    personalization_t* p = get_personalization();
    uint32_t text_clr = (p->theme == 0) ? 0xF0F6FC : 0x1A1A1A;
    uint32_t header_bg = (p->theme == 0) ? 0x222222 : 0xE8E8E8;
    uint32_t row_bg1 = (p->theme == 0) ? 0x1C2128 : 0xFAFAFA;
    uint32_t row_bg2 = (p->theme == 0) ? 0x181D23 : 0xF0F0F0;
    uint32_t sel_bg = (p->theme == 0) ? 0x2D2D2D : 0xDDDDDD;
    uint32_t accent = get_accent_color();

    int col_x[] = { 0, 40, 120, 200 };
    int col_h = 20;
    int row_h = 20;
    int visible_rows = h / row_h;

    // Headers
    gfx_fill_rect(x, y, w, col_h, header_bg);
    gfx_draw_string_transparent(x + col_x[0] + 4, y + 2, "PID", accent);
    gfx_draw_string_transparent(x + col_x[1] + 4, y + 2, "Name", accent);
    gfx_draw_string_transparent(x + col_x[2] + 4, y + 2, "State", accent);
    gfx_draw_string_transparent(x + col_x[3] + 4, y + 2, "Stack", accent);
    gfx_draw_hline(x, y + col_h, w, accent);

    // Task entries
    int tasks_found = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = get_task_by_index(i);
        if (!t || t->state == TASK_FREE) continue;
        if (tasks_found < pv_scroll) { tasks_found++; continue; }

        int row = tasks_found - pv_scroll;
        if (row >= visible_rows - 1) break;

        int ry = y + col_h + row * row_h + 2;
        uint32_t bg = (tasks_found % 2 == 0) ? row_bg1 : row_bg2;
        if (pv_selected == tasks_found && pv_kill_mode) bg = 0xF85149;

        gfx_fill_rect(x, ry, w, row_h, bg);

        // PID
        char buf[16];
        int pi = 0;
        int pid = t->id;
        if (pid == 0) { buf[0] = '0'; pi = 1; }
        else {
            int tmp = pid, digits = 0, p = pid;
            while (p > 0) { digits++; p /= 10; }
            pi = digits;
            while (pid > 0) { buf[--pi] = '0' + (pid % 10); pid /= 10; }
            pi = digits;
        }
        buf[pi] = 0;
        gfx_draw_string_transparent(x + col_x[0] + 4, ry + 2, buf, text_clr);
        gfx_draw_string_transparent(x + col_x[1] + 4, ry + 2, t->name, text_clr);

        const char* state_str = "Ready";
        if (t->state == TASK_RUNNING) state_str = "Running";
        else if (t->state == TASK_SLEEPING) state_str = "Sleep";
        else if (t->state == TASK_DEAD) state_str = "Dead";
        gfx_draw_string_transparent(x + col_x[2] + 4, ry + 2, state_str, (t->state == TASK_RUNNING) ? 0x3FB950 : text_clr);

        // Stack usage approx
        gfx_draw_string_transparent(x + col_x[3] + 4, ry + 2, "16KB", text_clr);

        if (pv_selected == tasks_found) {
            gfx_draw_rect_outline(x + 1, ry, w - 2, row_h, 1, accent);
        }

        tasks_found++;
    }

    // Footer hint
    char hint[64];
    int hi = 0;
    const char* hint_text = "Up/Down: Navigate | K: Kill selected | Q: Exit";
    while (hint_text[hi] && hi < 63) { hint[hi] = hint_text[hi]; hi++; }
    hint[hi] = 0;
    gfx_draw_string_transparent(x + 4, y + h - 18, hint, text_clr);
}

static void pv_key(int id, char key) {
    (void)id;
    if (key == 'q' || key == 'Q' || key == 27) {
        wm_close_window(pv_win_id);
        return;
    }
    if (KEY_MATCH((unsigned char)key, KEY_UP)) {
        if (pv_selected > 0) pv_selected--;
    }
    if (KEY_MATCH((unsigned char)key, KEY_DOWN)) {
        int max = 0;
        for (int i = 0; i < MAX_TASKS; i++) {
            task_t* t = get_task_by_index(i);
            if (t && t->state != TASK_FREE) max++;
        }
        if (pv_selected < max - 1) pv_selected++;
    }
    if (key == 'k' || key == 'K') {
        pv_kill_mode = 1;
    }
    if (key == '\n' && pv_kill_mode) {
        int idx = 0;
        for (int i = 0; i < MAX_TASKS; i++) {
            task_t* t = get_task_by_index(i);
            if (!t || t->state == TASK_FREE) continue;
            if (idx == pv_selected) {
                if (t->id > 1) exit_task(0);
                break;
            }
            idx++;
        }
        pv_kill_mode = 0;
    }
}

void process_viewer_app(void) {
    pv_scroll = 0;
    pv_selected = 0;
    pv_kill_mode = 0;
    pv_win_id = wm_open_window(200, 100, 500, 400, "Process Viewer", 0x3FB950, pv_render, pv_key, 0);
}

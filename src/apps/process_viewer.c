// ============================================================
//  BEDI OS — Process Viewer
//  Detailed task list with sort, filter, and kill controls
// ============================================================
#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "kernel/task/task.h"
#include "kernel/time/timer.h"
#include "drivers/video/framebuffer.h"
#include <stdint.h>
#include <string.h>

#define MAX_TASKS 64
#define ROW_H     20
#define HEADER_H  22
#define FOOTER_H  22

static int pv_win_id    = -1;
static int pv_scroll    = 0;
static int pv_selected  = 0;
static int pv_kill_mode = 0;
static int pv_sort_col  = 0;       /* 0=PID, 1=Name, 2=State, 3=Stack */
static int pv_filter    = 0;       /* 0=All, 1=Running, 2=Sleeping, 3=Kernel, 4=User */
static uint32_t pv_last_tick = 0;

static const char* state_name(task_state_t s) {
    if (s == TASK_RUNNING)  return "Running";
    if (s == TASK_SLEEPING) return "Sleeping";
    if (s == TASK_READY)    return "Ready";
    if (s == TASK_DEAD)     return "Dead";
    return "Free";
}

static int state_color(task_state_t s) {
    if (s == TASK_RUNNING)  return 0x3FB950;
    if (s == TASK_SLEEPING) return 0xD29922;
    if (s == TASK_READY)    return 0x58A6FF;
    if (s == TASK_DEAD)     return 0xF85149;
    return 0x6E7681;
}

static int pv_get_real_count(void) {
    int c = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = get_task_by_index(i);
        if (t && t->state != TASK_FREE) c++;
    }
    return c;
}

static int pv_match_filter(task_t* t) {
    if (pv_filter == 0) return 1;
    if (pv_filter == 1 && t->state == TASK_RUNNING)  return 1;
    if (pv_filter == 2 && t->state == TASK_SLEEPING) return 1;
    if (pv_filter == 3 && t->ring == TASK_RING0)      return 1;
    if (pv_filter == 4 && t->ring == TASK_RING3)      return 1;
    return 0;
}

/* Build a sorted+filtered index array for visible rows */
static void pv_build_list(int* out_idx, int max_out, int* out_count) {
    int tmp[MAX_TASKS];
    int n = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t* t = get_task_by_index(i);
        if (t && t->state != TASK_FREE && pv_match_filter(t)) {
            tmp[n++] = i;
        }
    }

    /* Simple bubble sort by selected column */
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            task_t* a = get_task_by_index(tmp[i]);
            task_t* b = get_task_by_index(tmp[j]);
            int swap = 0;
            if (pv_sort_col == 0) {
                if (b->id < a->id) swap = 1;
            } else if (pv_sort_col == 1) {
                if (strcmp(b->name, a->name) < 0) swap = 1;
            } else if (pv_sort_col == 2) {
                if (b->state < a->state) swap = 1;
            } else if (pv_sort_col == 3) {
                int sa = (int)(a->kernel_stack_top - a->kernel_stack_base);
                int sb = (int)(b->kernel_stack_top - b->kernel_stack_base);
                if (sb < sa) swap = 1;
            }
            if (swap) { int t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
        }
    }

    int count = n;
    if (count > max_out) count = max_out;
    for (int i = 0; i < count; i++) out_idx[i] = tmp[i];
    *out_count = count;
}

static void pv_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    uint32_t bg        = 0x0D1117;
    uint32_t header_bg = 0x161B22;
    uint32_t row_bg1   = 0x0D1117;
    uint32_t row_bg2   = 0x111820;
    uint32_t sel_bg    = 0x1A437A;
    uint32_t accent    = get_accent_color();
    uint32_t text_clr  = 0xC9D1D9;
    uint32_t mut_clr   = 0x6E7681;
    uint32_t border    = 0x30363D;

    gfx_fill_rect(x, y, w, h, bg);

    /* Column layout */
    int cx_name   = 60;
    int cx_ppid   = cx_name + 140;
    int cx_state  = cx_ppid + 60;
    int cx_ring   = cx_state + 70;
    int cx_stack  = cx_ring + 60;
    int cx_fds    = cx_stack + 70;

    /* Header */
    gfx_fill_rect(x, y, w, HEADER_H, header_bg);
    gfx_draw_hline(x, y + HEADER_H, w, border);

    gfx_draw_string_transparent(x + 4, y + 4, "PID", (pv_sort_col == 0) ? accent : mut_clr);
    gfx_draw_string_transparent(x + cx_name - x + 4, y + 4, "Name", (pv_sort_col == 1) ? accent : mut_clr);
    gfx_draw_string_transparent(x + cx_ppid - x + 4, y + 4, "PPID", (pv_sort_col == 0) ? mut_clr : 0x8B949E);
    gfx_draw_string_transparent(x + cx_state - x + 4, y + 4, "State", (pv_sort_col == 2) ? accent : mut_clr);
    gfx_draw_string_transparent(x + cx_ring - x + 4, y + 4, "Type", mut_clr);
    gfx_draw_string_transparent(x + cx_stack - x + 4, y + 4, "Stack", (pv_sort_col == 3) ? accent : mut_clr);
    gfx_draw_string_transparent(x + cx_fds - x + 4, y + 4, "FDs", mut_clr);

    /* Divider */
    gfx_draw_vline(x + cx_name - 4, y + 4, HEADER_H - 8, border);
    gfx_draw_vline(x + cx_ppid - 4, y + 4, HEADER_H - 8, border);
    gfx_draw_vline(x + cx_state - 4, y + 4, HEADER_H - 8, border);
    gfx_draw_vline(x + cx_ring - 4, y + 4, HEADER_H - 8, border);
    gfx_draw_vline(x + cx_stack - 4, y + 4, HEADER_H - 8, border);

    /* Filter indicator */
    if (pv_filter != 0) {
        const char* fl = "";
        if (pv_filter == 1) fl = "[Running]";
        else if (pv_filter == 2) fl = "[Sleeping]";
        else if (pv_filter == 3) fl = "[Kernel]";
        else if (pv_filter == 4) fl = "[User]";
        gfx_draw_string_transparent(x + w - 70, y + 4, fl, accent);
    }

    /* Build visible list */
    int list[MAX_TASKS];
    int list_count = 0;
    pv_build_list(list, MAX_TASKS, &list_count);

    int body_y  = y + HEADER_H;
    int body_h  = h - HEADER_H - FOOTER_H;
    int rows_vis = body_h / ROW_H;

    /* Scrollbar track */
    int sb_x = x + w - 12;
    gfx_fill_rect(sb_x, body_y, 12, body_h, 0x161B22);
    gfx_draw_vline(sb_x, body_y, body_h, border);
    gfx_draw_vline(sb_x + 11, body_y, body_h, border);

    int total = list_count;
    if (total < 1) total = 1;
    int thumb_h = (body_h * rows_vis) / total;
    if (thumb_h < 18) thumb_h = 18;
    int max_scroll = total - rows_vis;
    if (max_scroll < 0) max_scroll = 0;
    int thumb_y = body_y + (pv_scroll * (body_h - thumb_h)) / (max_scroll > 0 ? max_scroll : 1);
    gfx_fill_rect(sb_x + 1, thumb_y, 10, thumb_h, 0x30363D);

    int drawn = 0;
    for (int i = 0; i < list_count && drawn < rows_vis; i++) {
        int idx = list[i];
        task_t* t = get_task_by_index(idx);
        if (!t) continue;

        int row = i - pv_scroll;
        if (row < 0) continue;
        if (row >= rows_vis) break;

        int ry = body_y + row * ROW_H + 2;
        uint32_t bg = (idx % 2 == 0) ? row_bg1 : row_bg2;
        if (pv_selected == idx && pv_kill_mode) bg = 0x3D1F1F;
        if (pv_selected == idx) bg = sel_bg;

        gfx_fill_rect(x, ry, w - 14, ROW_H, bg);

        /* PID */
        char buf[16];
        int pi = 0, tmp = t->id;
        if (tmp == 0) { buf[0] = '0'; pi = 1; }
        else {
            int d = 0, p2 = tmp;
            while (p2 > 0) { d++; p2 /= 10; }
            pi = d;
            while (tmp > 0) { buf[--pi] = '0' + (tmp % 10); tmp /= 10; }
        }
        buf[pi] = 0;
        gfx_draw_string_transparent(x + 4, ry + 4, buf, text_clr);

        /* Name */
        gfx_draw_string_transparent(x + cx_name - x + 4, ry + 4, t->name, text_clr);

        /* PPID */
        char ppid_buf[8];
        itoa(t->ppid, ppid_buf);
        gfx_draw_string_transparent(x + cx_ppid - x + 4, ry + 4, ppid_buf, mut_clr);

        /* State */
        uint32_t sc = state_color(t->state);
        gfx_draw_string_transparent(x + cx_state - x + 4, ry + 4, state_name(t->state), sc);

        /* Type */
        const char* ring_s = (t->ring == TASK_RING0) ? "Kernel" : "User";
        gfx_draw_string_transparent(x + cx_ring - x + 4, ry + 4, ring_s, mut_clr);

        /* Stack usage */
        uint64_t used = (t->kernel_stack_top > t->kernel_stack_base)
                        ? (t->kernel_stack_top - t->kernel_stack_base)
                        : 0;
        char stack_buf[24];
        itoa((int)(used / 1024), stack_buf);
        int sl = 0;
        while (stack_buf[sl]) sl++;
        stack_buf[sl++] = 'K'; stack_buf[sl++] = 'B'; stack_buf[sl] = 0;
        gfx_draw_string_transparent(x + cx_stack - x + 4, ry + 4, stack_buf, mut_clr);

        /* Open FDs count */
        int fds_open = 0;
        for (int f = 0; f < MAX_FDS; f++) {
            if (t->fds[f]) fds_open++;
        }
        char fd_buf[8];
        itoa(fds_open, fd_buf);
        gfx_draw_string_transparent(x + cx_fds - x + 4, ry + 4, fd_buf, mut_clr);

        /* Selection outline */
        if (pv_selected == idx) {
            gfx_draw_rect_outline(x + 1, ry, w - 16, ROW_H, 1, accent);
        }

        drawn++;
    }

    if (list_count == 0) {
        gfx_draw_string_transparent(x + 20, body_y + body_h / 2 - 8, "No tasks match the current filter", 0x484F58);
    }

    /* Footer */
    int sb_y = y + h - FOOTER_H;
    gfx_fill_rect(x, sb_y, w, FOOTER_H, header_bg);
    gfx_draw_hline(x, sb_y, w, border);

    int total_tasks = pv_get_real_count();
    char total_buf[16];
    itoa(total_tasks, total_buf);
    gfx_draw_string_transparent(x + 4, sb_y + 4, "Total:", mut_clr);
    gfx_draw_string_transparent(x + 40, sb_y + 4, total_buf, text_clr);

    if (pv_selected >= 0 && pv_selected < MAX_TASKS) {
        task_t* st = get_task_by_index(pv_selected);
        if (st && st->state != TASK_FREE) {
            gfx_draw_string_transparent(x + 90, sb_y + 4, "Selected:", mut_clr);
            gfx_draw_string_transparent(x + 155, sb_y + 4, st->name, accent);
        }
    }

    gfx_draw_string_transparent(x + w - 160, sb_y + 4,
        pv_kill_mode ? "Kill mode: select + Enter" : "K: Kill | F: Filter | S: Sort | Q: Quit",
        pv_kill_mode ? accent : mut_clr);
}

static void pv_key(int id, char key) {
    (void)id;

    if (key == 'q' || key == 'Q' || key == 27) {
        wm_close_window(pv_win_id);
        return;
    }

    /* Refresh on R */
    if (key == 'r' || key == 'R') {
        pv_scroll = 0;
        pv_selected = 0;
        pv_kill_mode = 0;
        return;
    }

    /* Cycle filter */
    if (key == 'f' || key == 'F') {
        pv_filter++;
        if (pv_filter > 4) pv_filter = 0;
        pv_scroll = 0;
        pv_selected = 0;
        return;
    }

    /* Cycle sort column */
    if (key == 's' || key == 'S') {
        pv_sort_col++;
        if (pv_sort_col > 3) pv_sort_col = 0;
        pv_scroll = 0;
        pv_selected = 0;
        return;
    }

    int total_real = pv_get_real_count();
    int list[MAX_TASKS];
    int list_count = 0;
    pv_build_list(list, MAX_TASKS, &list_count);

    if (key == 'k' || key == 'K') {
        pv_kill_mode = !pv_kill_mode;
        return;
    }

    if (KEY_MATCH((unsigned char)key, KEY_UP)) {
        if (pv_selected > 0) {
            /* Move to previous visible index */
            for (int i = 0; i < list_count; i++) {
                if (list[i] == pv_selected && i > 0) {
                    pv_selected = list[i - 1];
                    break;
                }
            }
        }
    }
    if (KEY_MATCH((unsigned char)key, KEY_DOWN)) {
        for (int i = 0; i < list_count; i++) {
            if (list[i] == pv_selected && i < list_count - 1) {
                pv_selected = list[i + 1];
                break;
            }
        }
    }

    if (key == '\n' && pv_kill_mode) {
        for (int i = 0; i < list_count; i++) {
            if (list[i] == pv_selected) {
                task_t* t = get_task_by_index(pv_selected);
                if (t && t->id > 1) {
                    exit_task(0);
                }
                break;
            }
        }
        pv_kill_mode = 0;
    }

    /* Auto-scroll with selection */
    int body_h = 400 - HEADER_H - FOOTER_H; /* approx; actual handled by render */
    int rows_vis = body_h / ROW_H;
    int sel_row = -1;
    for (int i = 0; i < list_count; i++) {
        if (list[i] == pv_selected) { sel_row = i; break; }
    }
    if (sel_row >= 0) {
        if (sel_row < pv_scroll) pv_scroll = sel_row;
        else if (sel_row >= pv_scroll + rows_vis) pv_scroll = sel_row - rows_vis + 1;
    }
}

static void pv_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

void process_viewer_app(void) {
    if (wm_get_window(pv_win_id)) { wm_bring_to_front(pv_win_id); return; }

    pv_scroll = 0;
    pv_selected = 0;
    pv_kill_mode = 0;
    pv_filter = 0;
    pv_sort_col = 0;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    pv_win_id = wm_open_window(
        (fw - 700) / 2,
        (fh - 420) / 2,
        700,
        420,
        "Process Viewer",
        0x3FB950,
        pv_render,
        pv_key,
        pv_on_resize
    );
    wm_set_mouse_handler(pv_win_id, NULL);
}

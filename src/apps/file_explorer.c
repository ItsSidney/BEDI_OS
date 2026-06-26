// ============================================================
//  BEDI OS — Caja-inspired File Explorer
//  Full-featured with toolbar, sidebar, breadcrumbs, sorting,
//  context menus, clipboard, trash support, and file launching.
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "gui/gui.h"
#include "gui/wm.h"
#include "gui/icons.h"
#include "gui/app_icons.h"
#include "filesystem/filesystem.h"
#include "commands/commands.h"
#include "apps/text_editor.h"
#include "apps/imgview.h"
#include "kernel/time/timer.h"

#define MAX_FE_FILES 256
#define ROW_HEIGHT   26
#define TOOLBAR_H    32
#define PATHBAR_H    24
#define SIDEBAR_W    180
#define STATUSBAR_H  24
#define CONTENT_TOP  (TOOLBAR_H + PATHBAR_H)
#define HISTORY_MAX  32

typedef struct {
    char name[64];
    int size;
    int type;
    int parent;
    uint8_t flags;
    uint32_t mod_time;
} fe_file_entry_t;

typedef enum {
    SORT_NAME, SORT_SIZE, SORT_TYPE, SORT_MODIFIED, SORT_PERM
} fe_sort_col_t;

enum {
    PLACE_HOME, PLACE_DESKTOP, PLACE_DOCS, PLACE_DOWNLOADS,
    PLACE_MUSIC, PLACE_PICTURES, PLACE_TRASH, PLACE_ROOT,
    PLACE_COUNT
};

static fe_file_entry_t fe_files[MAX_FE_FILES];
static int fe_file_count = 0;
static int fe_win_id = -1;
static int fe_selected_idx = -1;
static int fe_scroll_offset = 0;
static int fe_prev_mouse = 0;
static int fe_hover_row = -1;
static fe_sort_col_t fe_sort_col = SORT_NAME;
static int fe_sort_asc = 1;
static char fe_current_path[256];

static char fe_history_paths[HISTORY_MAX][256];

static int fe_is_in_place(int place);
extern void desk_remove_icon_by_path(const char* path);
static void fe_disarm_delete(void);
static void fe_refresh(void);
static int fe_history_pos = -1;

static int fe_ctx_visible = 0;
static int fe_ctx_x, fe_ctx_y;
static int fe_ctx_sel = -1;

static char fe_clipboard_name[64];
static int fe_clipboard_cut = 0;
static int fe_clipboard_has = 0;

static int fe_history_count = 0;

static int fe_renaming = 0;
static char fe_rename_buf[64];
static int fe_rename_len = 0;

static int fe_creating = 0;
static char fe_create_buf[64];
static int fe_create_len = 0;

static int fe_props_visible = 0;

static uint32_t fe_last_click_time = 0;
static int fe_last_click_idx = -1;
static int fe_delete_armed = 0;
static uint32_t fe_delete_arm_time = 0;

static const char* fe_place_names[PLACE_COUNT] = {
    "Home", "Desktop", "Documents", "Downloads", "Music", "Pictures", "Trash", "File System"
};
static uint32_t fe_place_colors[PLACE_COUNT] = {
    0x58A6FF, 0x3FB950, 0xF0883E, 0x3FB950, 0xBC8CFF, 0xF778BA,
    0xF85149, 0xE3B341
};

static void format_size(int bytes, char* buf) {
    if (bytes < 1024) {
        itoa(bytes, buf);
        int len = strlen(buf);
        buf[len++] = ' '; buf[len++] = 'B'; buf[len] = 0;
    } else if (bytes < 1048576) {
        itoa(bytes / 1024, buf);
        int len = strlen(buf);
        buf[len++] = ' '; buf[len++] = 'K'; buf[len++] = 'B'; buf[len] = 0;
    } else {
        itoa(bytes / 1048576, buf);
        int len = strlen(buf);
        buf[len++] = ' '; buf[len++] = 'M'; buf[len++] = 'B'; buf[len] = 0;
    }
}

static int ends_with(const char* name, const char* ext) {
    int nl = strlen(name);
    int el = strlen(ext);
    if (nl < el) return 0;
    return (strcmp(name + nl - el, ext) == 0);
}

static int is_txt(const char* n) { return ends_with(n, ".txt"); }
static int is_bc(const char* n) { return ends_with(n, ".bc"); }
static int is_bin(const char* n) { return ends_with(n, ".bin"); }
static int is_bmp(const char* n) { return ends_with(n, ".bmp") || ends_with(n, ".BMP"); }
static int is_cfile(const char* n) { return ends_with(n, ".c") || ends_with(n, ".h"); }
static int is_asm(const char* n) { return ends_with(n, ".asm"); }

static const char* fe_get_type_name(const char* name, int is_dir) {
    if (is_dir) return "Folder";
    if (is_txt(name)) return "Text File";
    if (is_bc(name)) return "Bedi-C Source";
    if (is_bin(name)) return "Executable";
    if (is_bmp(name)) return "Bitmap";
    if (is_cfile(name)) { return ends_with(name, ".c") ? "C Source" : "C Header"; }
    if (is_asm(name)) return "Assembly";
    return "Binary";
}

static int fe_sort_compare(const fe_file_entry_t* a, const fe_file_entry_t* b) {
    if (a->type != b->type) {
        if (a->type == 1 && b->type == 2) return -1;
        if (a->type == 2 && b->type == 1) return 1;
    }
    int r = 0;
    switch (fe_sort_col) {
        case SORT_NAME: r = strcmp(a->name, b->name); break;
        case SORT_SIZE: r = a->size - b->size; break;
        case SORT_TYPE: {
            const char* ta = fe_get_type_name(a->name, a->type == 1);
            const char* tb = fe_get_type_name(b->name, b->type == 1);
            r = strcmp(ta, tb); break;
        }
        case SORT_MODIFIED: r = (a->mod_time > b->mod_time) ? 1 : (a->mod_time < b->mod_time) ? -1 : 0; break;
        case SORT_PERM: {
            char pa[16], pb[16];
            fs_get_permission_string(a->flags, pa);
            fs_get_permission_string(b->flags, pb);
            r = strcmp(pa, pb); break;
        }
    }
    return fe_sort_asc ? r : -r;
}

static void fe_sort_files(void) {
    for (int i = 0; i < fe_file_count - 1; i++) {
        for (int j = 0; j < fe_file_count - 1 - i; j++) {
            if (fe_sort_compare(&fe_files[j], &fe_files[j + 1]) > 0) {
                fe_file_entry_t t = fe_files[j];
                fe_files[j] = fe_files[j + 1];
                fe_files[j + 1] = t;
            }
        }
    }
}

static void fe_navigate_to_path(const char* path) {
    fs_cd(path);
    fe_refresh();
}

static void fe_add_history(void) {
    int new_count = fe_history_pos + 1;
    fe_history_count = (new_count < HISTORY_MAX) ? new_count : HISTORY_MAX;
    fe_history_pos = fe_history_count;
    fs_pwd(fe_history_paths[fe_history_pos], 256);
    fe_history_count = fe_history_pos + 1;
}

static void fe_load_files(void) {
    fe_file_count = 0;
    int curr_dir = fs_get_current_dir();
    if (curr_dir < 0) return;
    int count = fs_get_dir_count(curr_dir);
    if (count > MAX_FE_FILES) count = MAX_FE_FILES;
    for (int i = 0; i < count; i++) {
        char name[64]; int sz, tp; uint8_t fl; uint32_t mt;
        if (fs_find_by_index(curr_dir, i, name, &sz, &tp, &fl, &mt) == 0) {
            fe_file_entry_t* e = &fe_files[fe_file_count];
            strcpy(e->name, name);
            e->size = sz; e->type = tp; e->parent = curr_dir;
            e->flags = fl; e->mod_time = mt;
            fe_file_count++;
        }
    }
    fe_sort_files();
}

static void fe_refresh(void) {
    fe_load_files();
    fe_selected_idx = -1;
    fe_scroll_offset = 0;
    fe_disarm_delete();
    fs_pwd(fe_current_path, 256);
}

static void fe_change_dir(const char* name) {
    if (name[0] == '/') {
        fs_cd(name);
    } else if (strcmp(name, "..") == 0) {
        fs_cd("..");
    } else {
        fs_cd(name);
    }
    fe_refresh();
    fe_add_history();
}

static void fe_go_home(void) {
    int home = fs_get_home_dir();
    if (home >= 0) {
        char name[64]; int sz, tp, par; uint8_t fl; uint32_t mt;
        if (fs_get_node(home, name, &sz, &tp, &par, &fl, &mt) == 0) {
            char path[256] = "/";
            int pi = 1;
            for (int j = 0; name[j] && pi < 254; j++) path[pi++] = name[j];
            path[pi] = 0;
            fs_cd(path);
        } else {
            fs_cd("/");
        }
    } else {
        fs_cd("/");
    }
    fe_refresh();
    fe_add_history();
}

static void fe_go_root(void) {
    fs_cd("/");
    fe_refresh();
    fe_add_history();
}

static void fe_go_place(int place) {
    if (place == PLACE_ROOT) { fe_go_root(); return; }
    int home = fs_get_home_dir();
    if (home < 0) return;
    char home_name[64]; int sz, tp, par; uint8_t fl; uint32_t mt;
    if (fs_get_node(home, home_name, &sz, &tp, &par, &fl, &mt) != 0) return;
    char path[256] = "/";
    int pi = 1;
    for (int j = 0; home_name[j] && pi < 254; j++) path[pi++] = home_name[j];
    if (place == PLACE_HOME) { path[pi] = 0; fs_cd(path); fe_refresh(); fe_add_history(); return; }
    if (place == PLACE_DESKTOP || place == PLACE_TRASH) {
        int ti = pi;
        path[ti++] = '/';
        const char* sn = (place == PLACE_DESKTOP) ? "Desktop" : "Trash";
        for (int j = 0; sn[j] && ti < 254; j++) path[ti++] = sn[j];
        path[ti] = 0;
        if (fs_cd(path) == 0) { fe_refresh(); fe_add_history(); }
        return;
    }
    path[pi++] = '/';
    const char* sub = "";
    if (place == PLACE_DOCS) sub = "Documents";
    else if (place == PLACE_DOWNLOADS) sub = "Downloads";
    else if (place == PLACE_MUSIC) sub = "Music";
    else if (place == PLACE_PICTURES) sub = "Pictures";
    for (int j = 0; sub[j] && pi < 254; j++) path[pi++] = sub[j];
    path[pi] = 0;
    if (sub[0] && fs_cd(path) == 0) {
        fe_refresh();
        fe_add_history();
    }
}

static int fe_is_in_trash(void) {
    int cur = fs_get_current_dir();
    int trash = fs_get_trash_dir();
    return (cur == trash);
}

static void fe_draw_icon(int ix, int iy, const char* name, int is_dir) {
    if (is_dir) {
        gfx_fill_rect(ix + 2, iy + 8, 16, 11, 0xEBCB8B);
        gfx_draw_rect_outline(ix + 2, iy + 8, 16, 11, 1, 0xD4A74D);
        gfx_fill_rect(ix + 4, iy + 8, 4, 3, 0xD4A74D);
        return;
    }
    if (is_txt(name)) {
        gfx_fill_rect(ix + 2, iy + 2, 18, 20, 0xE6EDF3);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 20, 1, 0x8B949E);
        gfx_fill_rect(ix + 5, iy + 5, 6, 2, 0x8B949E);
        gfx_fill_rect(ix + 5, iy + 9, 12, 2, 0x8B949E);
        gfx_fill_rect(ix + 5, iy + 13, 8, 2, 0x8B949E);
    } else if (is_bc(name)) {
        gfx_fill_rect(ix + 2, iy + 2, 18, 20, 0xBC8CFF);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 20, 1, 0xD2A8FF);
        gfx_fill_rect(ix + 4, iy + 2, 4, 4, 0xD2A8FF);
        gfx_draw_string_transparent(ix + 7, iy + 4, "B", 0xFFFFFF);
        gfx_draw_string_transparent(ix + 7, iy + 16, ".", 0xFFFFFF);
    } else if (is_bin(name)) {
        gfx_fill_rect(ix + 2, iy + 2, 12, 18, 0x238636);
        gfx_fill_rect(ix + 14, iy + 6, 6, 10, 0x238636);
        gfx_draw_rect_outline(ix + 2, iy + 2, 12, 18, 1, 0x3FB950);
        gfx_draw_rect_outline(ix + 14, iy + 6, 6, 10, 1, 0x3FB950);
        gfx_fill_rect(ix + 7, iy + 8, 2, 6, 0xFFFFFF);
    } else if (is_bmp(name)) {
        gfx_fill_rect(ix + 2, iy + 2, 18, 18, 0x1D1F26);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 18, 1, 0x58A6FF);
        gfx_fill_rect(ix + 12, iy + 4, 6, 4, 0x22D3EE);
        gfx_fill_circle(ix + 6, iy + 8, 3, 0xFBBF24);
        gfx_fill_rect(ix + 4, iy + 14, 14, 4, 0x3FB950);
        gfx_fill_rect(ix + 8, iy + 10, 4, 5, 0x4D5059);
    } else if (is_cfile(name)) {
        uint32_t bg = ends_with(name, ".c") ? 0x1F6FEB : 0x58A6FF;
        gfx_fill_rect(ix + 2, iy + 2, 18, 20, bg);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 20, 1, 0x79C0FF);
        gfx_fill_rect(ix + 5, iy + 5, 4, 2, 0xFFFFFF);
        gfx_fill_rect(ix + 5, iy + 11, 4, 2, 0xFFFFFF);
        gfx_fill_rect(ix + 5, iy + 16, 8, 2, 0xFFFFFF);
        char l = ends_with(name, ".c") ? 'C' : 'H';
        char s[2] = {l, 0};
        gfx_draw_string_transparent(ix + 10, iy + 4, s, 0xFFFFFF);
    } else if (is_asm(name)) {
        gfx_fill_rect(ix + 2, iy + 2, 18, 20, 0x484F58);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 20, 1, 0x6E7681);
        gfx_draw_hline(ix + 5, iy + 5, 12, 0x8B949E);
        gfx_draw_hline(ix + 8, iy + 9, 6, 0x8B949E);
        gfx_draw_hline(ix + 5, iy + 13, 12, 0x8B949E);
        gfx_draw_hline(ix + 8, iy + 17, 6, 0x8B949E);
    } else {
        gfx_fill_rect(ix + 2, iy + 2, 18, 20, 0x161B22);
        gfx_draw_rect_outline(ix + 2, iy + 2, 18, 20, 1, 0x58A6FF);
        gfx_fill_circle(ix + 11, iy + 7, 4, 0x58A6FF);
        gfx_fill_rect(ix + 7, iy + 13, 8, 6, 0x58A6FF);
    }
}

static void fe_ctx_show(int mx, int my) {
    fe_ctx_visible = 1;
    fe_ctx_x = mx;
    fe_ctx_y = my;
    fe_ctx_sel = -1;
}

static void fe_ctx_hide(void) {
    fe_ctx_visible = 0;
    fe_ctx_sel = -1;
}

static void fe_open_selected(void) {
    if (fe_selected_idx < 0 || fe_selected_idx >= fe_file_count) return;
    const char* n = fe_files[fe_selected_idx].name;
    if (fe_files[fe_selected_idx].type == 1) {
        fe_change_dir(n);
    } else if (is_bmp(n)) {
        imgview_open(n);
    } else if (is_bin(n)) {
        extern void brun_main(char*);
        brun_main((char*)n);
    } else {
        text_editor_open(n);
    }
}

static int fe_is_protected_dir(void) {
    int cur = fs_get_current_dir();
    if (cur == 0) return 1;
    int home = fs_get_home_dir();
    if (cur == home) return 1;
    int trash = fs_get_trash_dir();
    if (cur == trash) return 1;
    return 0;
}

static void fe_disarm_delete(void) {
    fe_delete_armed = 0;
}

static int fe_check_delete_arm(void) {
    if (fe_delete_armed) {
        fe_disarm_delete();
        return 1;
    }
    fe_delete_armed = 1;
    fe_delete_arm_time = timer_get_ms();
    return 0;
}

static void fe_check_disarm_auto(void) {
    if (fe_delete_armed && timer_get_ms() - fe_delete_arm_time > 2000)
        fe_disarm_delete();
}

static void fe_trash_selected(void) {
    if (fe_selected_idx < 0) return;
    if (fe_is_protected_dir()) return;
    if (!fe_check_delete_arm()) return;
    const char* n = fe_files[fe_selected_idx].name;
    if (fe_is_in_trash())
        fs_delete(n);
    else
        fs_trash_file(n);
    if (fe_is_in_place(PLACE_DESKTOP)) {
        desk_remove_icon_by_path(n);
    }
    fe_refresh();
}

static void fe_delete_selected(void) {
    if (fe_selected_idx < 0) return;
    if (fe_is_protected_dir()) return;
    if (!fe_check_delete_arm()) return;
    fs_delete(fe_files[fe_selected_idx].name);
    if (fe_is_in_place(PLACE_DESKTOP)) {
        desk_remove_icon_by_path(fe_files[fe_selected_idx].name);
    }
    fe_refresh();
}

static void fe_rename_selected(void) {
    if (fe_selected_idx < 0) return;
    fe_renaming = 1;
    fe_rename_len = 0;
    fe_rename_buf[0] = 0;
}

static void fe_do_rename(void) {
    if (fe_selected_idx < 0 || fe_rename_len == 0) { fe_renaming = 0; return; }
    fs_rename(fe_files[fe_selected_idx].name, fe_rename_buf);
    if (fe_is_in_place(PLACE_DESKTOP)) {
        desk_remove_icon_by_path(fe_files[fe_selected_idx].name);
    }
    fe_renaming = 0;
    fe_refresh();
}

static void fe_copy_selected(void) {
    if (fe_selected_idx < 0) return;
    strcpy(fe_clipboard_name, fe_files[fe_selected_idx].name);
    fe_clipboard_cut = 0;
    fe_clipboard_has = 1;
}

static void fe_cut_selected(void) {
    if (fe_selected_idx < 0) return;
    strcpy(fe_clipboard_name, fe_files[fe_selected_idx].name);
    fe_clipboard_cut = 1;
    fe_clipboard_has = 1;
}

static void fe_paste_clipboard(void) {
    if (!fe_clipboard_has) return;
    if (fe_clipboard_cut)
        fs_move(fe_clipboard_name, fe_clipboard_name);
    else
        fs_copy_file(fe_clipboard_name, fe_clipboard_name);
    if (fe_is_in_place(PLACE_DESKTOP)) {
        desk_add_file_icon(fe_clipboard_name, fe_clipboard_name);
    }
    fe_clipboard_has = 0;
    fe_refresh();
}

static void fe_empty_trash(void) {
    fs_empty_trash();
    fe_refresh();
}

static void fe_new_folder(void) {
    fe_creating = 1;
    fe_create_len = 0;
    fe_create_buf[0] = 0;
}

static void fe_new_file(void) {
    fe_creating = 2;
    fe_create_len = 0;
    fe_create_buf[0] = 0;
}

static void fe_do_create(void) {
    if (fe_create_len == 0) { fe_creating = 0; return; }
    if (fe_creating == 1)
        fs_mkdir(fe_create_buf);
    else
        fs_create(fe_create_buf);
    if (fe_is_in_place(PLACE_DESKTOP)) {
        desk_add_file_icon(fe_create_buf, fe_create_buf);
    }
    fe_creating = 0;
    fe_refresh();
}

static void fe_show_properties(void) {
    fe_props_visible = 1;
}

static const char* fe_ctx_item_label(int idx, int in_trash) {
    if (!in_trash && idx >= 12) idx++;
    static const char* labels[] = {
        "New Folder", "New File", "",
        "Copy", "Cut", "Paste", "",
        "Delete", "Rename", "Properties",
        "Add to Desktop", "",
        "Empty Trash"
    };
    if (idx < 0 || idx >= 13) return "";
    return labels[idx];
}

static void fe_add_to_desktop(void) {
    if (fe_selected_idx < 0 || fe_selected_idx >= fe_file_count) return;
    char path[256];
    fs_pwd(path, 256);
    int plen = strlen(path);
    if (plen > 0 && path[plen-1] != '/') { path[plen] = '/'; plen++; path[plen] = 0; }
    int si;
    for (si = 0; fe_files[fe_selected_idx].name[si] && plen + si < 250; si++)
        path[plen + si] = fe_files[fe_selected_idx].name[si];
    path[plen + si] = 0;
    desk_add_file_icon(path, fe_files[fe_selected_idx].name);
}

static void fe_ctx_execute(int idx, int in_trash) {
    if (!in_trash && idx >= 12) idx++;
    fe_ctx_hide();
    switch (idx) {
        case 0: fe_new_folder(); break;
        case 1: fe_new_file(); break;
        case 3: fe_copy_selected(); break;
        case 4: fe_cut_selected(); break;
        case 5: fe_paste_clipboard(); break;
        case 7: fe_trash_selected(); break;
        case 8: fe_rename_selected(); break;
        case 9: fe_show_properties(); break;
        case 10: fe_add_to_desktop(); break;
        case 12: fe_empty_trash(); break;
    }
}

// ── Drawing ─────────────────────────────────────────────────────

static void fe_draw_toolbar(int x, int y, int w) {
    gfx_fill_rect(x, y, w, TOOLBAR_H, 0x0D1117);
    gfx_draw_hline(x, y + TOOLBAR_H - 1, w, 0x21262D);
    int by = y + 2;
    int bw = 28, gap = 4, bh = 28;
    int bx0 = x + 6;
    uint32_t btn_bg = 0x0D1117;
    uint32_t btn_hv = 0x1A437A;
    int mx = mouse_get_x(), my = mouse_get_y();
    for (int i = 0; i < 6; i++) {
        int bx = bx0 + i * (bw + gap);
        int disabled = 0;
        if (i == 0 && fe_history_pos <= 0) disabled = 1;
        if (i == 1 && fe_history_pos >= fe_history_count - 1) disabled = 1;
        int hover = (!disabled && mx >= bx && mx < bx + bw && my >= by && my < by + bh);
        uint32_t bg = hover ? btn_hv : btn_bg;
        if (disabled) bg = 0x161B22;
        gfx_fill_rect(bx, by, bw, bh, bg);
        gfx_draw_rect_outline(bx, by, bw, bh, 1, hover ? 0x1F6FEB : 0x21262D);
        const char* lbl = "";
        if (i == 0) lbl = "<";
        else if (i == 1) lbl = ">";
        else if (i == 2) lbl = "^";
        else if (i == 3) lbl = "R";
        else if (i == 4) lbl = "H";
        else if (i == 5) lbl = "/";
        uint32_t tc = disabled ? 0x484F58 : (hover ? 0xFFFFFF : 0xC9D1D9);
        gfx_draw_string_transparent(bx + 10, by + 6, lbl, tc);
    }
}

static void fe_draw_pathbar(int x, int y, int w) {
    gfx_fill_rect(x, y, w, PATHBAR_H, 0x0D1117);
    gfx_draw_hline(x, y + PATHBAR_H - 1, w, 0x21262D);
    char path[256];
    fs_pwd(path, 256);
    int pl = strlen(path);
    int dx = x + 8;
    int seg_start = 0;
    for (int i = 0; i <= pl; i++) {
        if (path[i] == '/' || path[i] == 0) {
            if (i > seg_start || path[i] == '/') {
                char seg[64];
                int si = 0;
                if (seg_start == 0 && path[0] == '/') {
                    seg[0] = '/'; seg[1] = 0;
                } else {
                    for (int j = seg_start; j < i; j++)
                        seg[si++] = path[j];
                    seg[si] = 0;
                }
                    if (seg[0]) {
                        int segw = strlen(seg) * 8;
                        if (dx + segw + 12 < x + w) {
                            gfx_draw_string_transparent(dx, y + 4, seg, 0xCCCCCC);
                            dx += segw + 4;
                            if (path[i] != 0) {
                                gfx_draw_string_transparent(dx, y + 4, ">", 0x484F58);
                                dx += 12;
                            }
                        } else if (dx < x + w) {
                            // Last visible segment - draw as active
                            gfx_draw_string_transparent(dx, y + 4, seg, 0xE4E6EA);
                            dx += segw + 4;
                        }
                    }
            }
            seg_start = i + 1;
        }
    }
}

static int fe_is_in_place(int place) {
    int cur = fs_get_current_dir();
    if (place == PLACE_ROOT) return (cur == 0);
    int home = fs_get_home_dir();
    if (home < 0) return 0;
    char home_name[64]; int sz, tp, par; uint8_t fl; uint32_t mt;
    if (fs_get_node(home, home_name, &sz, &tp, &par, &fl, &mt) != 0) return 0;
    char path[256] = "/";
    int pi = 1;
    for (int j = 0; home_name[j] && pi < 254; j++) path[pi++] = home_name[j];
    if (place == PLACE_HOME) {
        path[pi] = 0;
        char cur_path[256];
        fs_pwd(cur_path, 256);
        int match = 1;
        for (int j = 0; path[j] && cur_path[j]; j++)
            if (path[j] != cur_path[j]) { match = 0; break; }
        return match;
    }
    if (place == PLACE_DESKTOP || place == PLACE_TRASH) {
        int ti = pi; path[ti++] = '/';
        const char* sn = (place == PLACE_DESKTOP) ? "Desktop" : "Trash";
        for (int j = 0; sn[j] && ti < 254; j++) path[ti++] = sn[j];
        path[ti] = 0;
        char cur_path[256];
        fs_pwd(cur_path, 256);
        int match = 1;
        for (int j = 0; path[j] && cur_path[j]; j++)
            if (path[j] != cur_path[j]) { match = 0; break; }
        return match;
    }
    path[pi++] = '/';
    const char* sub = "";
    if (place == PLACE_DOCS) sub = "Documents";
    else if (place == PLACE_DOWNLOADS) sub = "Downloads";
    else if (place == PLACE_MUSIC) sub = "Music";
    else if (place == PLACE_PICTURES) sub = "Pictures";
    for (int j = 0; sub[j] && pi < 254; j++) path[pi++] = sub[j];
    path[pi] = 0;
    if (!sub[0]) return 0;
    char cur_path[256];
    fs_pwd(cur_path, 256);
    int match = 1;
    for (int j = 0; path[j] && cur_path[j]; j++)
        if (path[j] != cur_path[j]) { match = 0; break; }
    return match;
}

static void fe_draw_sidebar(int x, int y, int h) {
    gfx_fill_rect(x, y, SIDEBAR_W + 1, h, 0x0D1117);
    gfx_draw_vline(x + SIDEBAR_W, y, h, 0x21262D);
    gfx_fill_rect(x, y, SIDEBAR_W, 30, 0x0D1117);
    gfx_draw_string_transparent(x + 10, y + 8, "Places", 0x8B949E);
    gfx_draw_hline(x + 6, y + 28, SIDEBAR_W - 12, 0x21262D);
    int cur = fs_get_current_dir();
    int mx = mouse_get_x(), my = mouse_get_y();
    for (int i = 0; i < PLACE_COUNT; i++) {
        int iy = y + 34 + i * 26;
        int hover = (mx >= x && mx < x + SIDEBAR_W && my >= iy && my < iy + 26);
        int active = fe_is_in_place(i);
        uint32_t bg = 0x0D1117;
        if (active) bg = 0x1A437A;
        else if (hover) bg = 0x161B22;
        gfx_fill_rect(x + 2, iy, SIDEBAR_W - 3, 26, bg);
        if (active) {
            gfx_fill_rect(x, iy, 3, 26, fe_place_colors[i]);
        }
        uint32_t c = fe_place_colors[i];
        int ix = x + 10, iiy = iy + 6;
        // Draw individual place icons
        if (i == PLACE_HOME) {
            gfx_draw_line(ix+6, iiy+11, ix+6, iiy+16, c);
            gfx_draw_line(ix+10, iiy+11, ix+10, iiy+16, c);
            gfx_draw_line(ix+4, iiy+11, ix+12, iiy+11, c);
            gfx_draw_line(ix+3, iiy+9, ix+8, iiy+4, c);
            gfx_draw_line(ix+8, iiy+4, ix+13, iiy+9, c);
        } else if (i == PLACE_DESKTOP) {
            gfx_draw_rect_outline(ix+3, iiy+4, 10, 9, 1, c);
            gfx_draw_hline(ix+5, iiy+14, 6, c);
            gfx_draw_rect_outline(ix+4, iiy+13, 8, 2, 1, c);
        } else if (i == PLACE_DOCS) {
            gfx_draw_line(ix+3, iiy+15, ix+3, iiy+6, c);
            gfx_draw_line(ix+3, iiy+6, ix+12, iiy+6, c);
            gfx_draw_line(ix+12, iiy+6, ix+13, iiy+15, c);
            gfx_draw_line(ix+3, iiy+15, ix+13, iiy+15, c);
            gfx_draw_line(ix+5, iiy+9, ix+11, iiy+9, c);
            gfx_draw_line(ix+5, iiy+12, ix+10, iiy+12, c);
        } else if (i == PLACE_DOWNLOADS) {
            gfx_draw_line(ix+8, iiy+3, ix+8, iiy+14, c);
            gfx_draw_line(ix+4, iiy+10, ix+8, iiy+14, c);
            gfx_draw_line(ix+12, iiy+10, ix+8, iiy+14, c);
            gfx_draw_line(ix+3, iiy+16, ix+13, iiy+16, c);
        } else if (i == PLACE_MUSIC) {
            gfx_fill_circle(ix+4, iiy+11, 3, c);
            gfx_draw_line(ix+7, iiy+11, ix+7, iiy+4, c);
            gfx_draw_line(ix+7, iiy+4, ix+13, iiy+5, c);
            gfx_fill_circle(ix+11, iiy+12, 3, c);
            gfx_draw_line(ix+10, iiy+12, ix+10, iiy+6, c);
        } else if (i == PLACE_PICTURES) {
            gfx_fill_rect(ix+3, iiy+6, 10, 10, c);
            gfx_draw_rect_outline(ix+3, iiy+6, 10, 10, 1, gfx_darken(c, 40));
            gfx_fill_circle(ix+6, iiy+9, 2, gfx_lighten(c, 40));
            gfx_fill_rect(ix+3, iiy+13, 10, 3, gfx_lighten(c, 30));
            gfx_fill_rect(ix+10, iiy+10, 3, 3, gfx_darken(c, 20));
        } else if (i == PLACE_TRASH) {
            gfx_fill_rect(ix+4, iiy+8, 8, 9, c);
            gfx_fill_rect(ix+3, iiy+7, 10, 2, c);
            gfx_draw_hline(ix+5, iiy+10, 6, gfx_darken(c, 40));
            gfx_fill_rect(ix+5, iiy+6, 6, 2, c);
            gfx_draw_rect_outline(ix+5, iiy+6, 6, 2, 1, gfx_darken(c, 40));
        } else if (i == PLACE_ROOT) {
            gfx_fill_rect(ix+5, iiy+4, 6, 13, c);
            gfx_fill_rect(ix+3, iiy+14, 10, 3, c);
            gfx_draw_rect_outline(ix+5, iiy+4, 6, 13, 1, gfx_darken(c, 40));
            gfx_fill_rect(ix+6, iiy+12, 4, 3, gfx_lighten(c, 40));
        }
        uint32_t tc = active ? 0xFFFFFF : 0xC9D1D9;
        gfx_draw_string_transparent(x + 26, iy + 6, fe_place_names[i], tc);
    }
}

static int fe_draw_mainview(int x, int y, int w, int h) {
    gfx_fill_rect(x, y, w, h, 0x0D1117);
    gfx_draw_vline(x + w - 1, y, h, 0x21262D);
    int col_hdr_h = 22;
    int col_x[5], col_w[5];
    col_w[0] = w * 34 / 100; if (col_w[0] < 100) col_w[0] = 100;
    col_w[1] = w * 12 / 100; if (col_w[1] < 55) col_w[1] = 55;
    col_w[2] = w * 20 / 100; if (col_w[2] < 70) col_w[2] = 70;
    col_w[3] = w * 20 / 100; if (col_w[3] < 80) col_w[3] = 80;
    col_w[4] = w - col_w[0] - col_w[1] - col_w[2] - col_w[3] - 8;
    col_x[0] = x + 4;
    for (int i = 1; i < 5; i++) col_x[i] = col_x[i - 1] + col_w[i - 1];
    gfx_fill_rect(x, y, w, col_hdr_h, 0x161B22);
    gfx_draw_hline(x, y + col_hdr_h - 1, w, 0x30363D);
    static const char* col_names[] = {"Name", "Size", "Type", "Modified", "Perms"};
    for (int c = 0; c < 5; c++) {
        uint32_t tc = 0x8B949E;
        gfx_draw_string_transparent(col_x[c] + 4, y + 3, col_names[c], tc);
        if (c == (int)fe_sort_col) {
            char ind = fe_sort_asc ? '\x18' : '\x1F';
            int nl = strlen(col_names[c]) * 8;
            char sind[2] = {ind, 0};
            gfx_draw_string_transparent(col_x[c] + 4 + nl + 2, y + 3, sind, 0x1F6FEB);
        }
        if (c < 4) gfx_draw_vline(col_x[c + 1] - 1, y, col_hdr_h, 0x21262D);
    }
    int body_y = y + col_hdr_h;
    int body_h = h - col_hdr_h;
    int visible_rows = body_h / ROW_HEIGHT;
    if (visible_rows < 0) visible_rows = 0;
    int mx = mouse_get_x(), my = mouse_get_y();
    fe_hover_row = -1;
    for (int r = 0; r < visible_rows; r++) {
        int idx = fe_scroll_offset + r;
        if (idx >= fe_file_count) break;
        int row_y = body_y + r * ROW_HEIGHT;
        int is_selected = (fe_selected_idx == idx);
        int hover = (mx >= x && mx < x + w && my >= row_y && my < row_y + ROW_HEIGHT);
        if (hover) fe_hover_row = idx;
        if (is_selected) {
            gfx_fill_rect(x + 2, row_y, w - 4, ROW_HEIGHT, 0x1A437A);
            gfx_draw_hline(x + 4, row_y + ROW_HEIGHT - 1, w - 8, 0x1F6FEB);
        } else if (hover) {
            gfx_fill_rect(x + 2, row_y, w - 4, ROW_HEIGHT, 0x161B22);
            gfx_draw_hline(x + 4, row_y + ROW_HEIGHT - 1, w - 8, 0x21262D);
        } else if (r % 2 == 1) {
            gfx_fill_rect(x + 2, row_y, w - 4, ROW_HEIGHT, 0x0A0E14);
        } else {
            gfx_fill_rect(x + 2, row_y, w - 4, ROW_HEIGHT, 0x0D1117);
        }
        const fe_file_entry_t* e = &fe_files[idx];
        int is_dir = (e->type == 1);
        fe_draw_icon(x + 6, row_y + 4, e->name, is_dir);
        uint32_t name_clr = is_selected ? 0xFFFFFF : (is_dir ? 0x58A6FF : 0xC9D1D9);
        char name_disp[40];
        int nl = strlen(e->name);
        int max_chars = (col_w[0] - 28) / 8;
        if (max_chars < 2) max_chars = 2;
        if (nl > max_chars) {
            for (int j = 0; j < max_chars - 3; j++) name_disp[j] = e->name[j];
            name_disp[max_chars - 3] = '.'; name_disp[max_chars - 2] = '.'; name_disp[max_chars - 1] = '.';
            name_disp[max_chars] = 0;
        } else {
            strcpy(name_disp, e->name);
        }
        gfx_draw_string_transparent(col_x[0] + 26, row_y + 5, name_disp, name_clr);
        char buf[24];
        if (col_w[1] > 40 && !is_dir) {
            format_size(e->size, buf);
            gfx_draw_string_transparent(col_x[1] + 4, row_y + 5, buf, 0x8B949E);
        } else if (col_w[1] > 40) {
            gfx_draw_string_transparent(col_x[1] + 4, row_y + 5, "--", 0x484F58);
        }
        if (col_w[2] > 50) {
            const char* tn = fe_get_type_name(e->name, is_dir);
            gfx_draw_string_transparent(col_x[2] + 4, row_y + 5, tn, 0x6E7681);
        }
        if (col_w[3] > 60) {
            fs_format_time(e->mod_time, buf, 24);
            gfx_draw_string_transparent(col_x[3] + 4, row_y + 5, buf, 0x6E7681);
        }
        if (col_w[4] > 40) {
            fs_get_permission_string(e->flags, buf);
            gfx_draw_string_transparent(col_x[4] + 4, row_y + 5, buf, 0x484F58);
        }
    }
    return visible_rows;
}

static void fe_draw_statusbar(int x, int y, int w) {
    gfx_fill_rect(x, y, w, STATUSBAR_H, 0x161B22);
    gfx_draw_hline(x, y, w, 0x30363D);
    if (fe_delete_armed) {
        gfx_draw_string_transparent(x + 8, y + 4, "Press again to confirm delete", 0xF87171);
        return;
    }
    int dirs = 0, files = 0;
    for (int i = 0; i < fe_file_count; i++) {
        if (fe_files[i].type == 1) dirs++;
        else files++;
    }
    char status[64];
    int si = 0;
    itoa(fe_file_count, status + si);
    while (status[si]) si++;
    status[si++] = ' '; status[si++] = 'i'; status[si++] = 't'; status[si++] = 'e'; status[si++] = 'm';
    if (fe_file_count != 1) { status[si++] = 's'; }
    status[si] = 0;
    gfx_draw_string_transparent(x + 8, y + 4, status, 0x8B949E);
    if (w > 200) {
        char detail[48];
        int di = 0;
        detail[di++] = '(';
        itoa(dirs, detail + di);
        while (detail[di]) di++;
        detail[di++] = ' '; detail[di++] = 'd'; detail[di++] = 'i'; detail[di++] = 'r'; detail[di++] = 's';
        detail[di++] = ',';
        itoa(files, detail + di);
        while (detail[di]) di++;
        detail[di++] = ' '; detail[di++] = 'f'; detail[di++] = 'i'; detail[di++] = 'l'; detail[di++] = 'e'; detail[di++] = 's';
        detail[di++] = ')'; detail[di] = 0;
        gfx_draw_string_transparent(x + 150, y + 4, detail, 0x6E7681);
    }
    if (w > 350) {
        gfx_draw_string_transparent(x + w - 60, y + 4, "List", 0x484F58);
    }
}

static void fe_draw_context_menu(void) {
    if (!fe_ctx_visible) return;
    int in_trash = fe_is_in_trash();
    int item_count = 13;
    if (!in_trash) item_count--;
    int menu_w = 170;
    int menu_h = item_count * 22 + 4;
    int mx = fe_ctx_x;
    int my = fe_ctx_y;
    int fw = get_fb_width(), fh = get_fb_height();
    if (mx + menu_w > fw) mx = fw - menu_w - 4;
    if (my + menu_h > fh) my = fh - menu_h - 4;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    gfx_fill_rect(mx, my, menu_w, menu_h, 0x1D1F26);
    gfx_draw_rect_outline(mx, my, menu_w, menu_h, 1, 0x30363D);
    int mmx = mouse_get_x(), mmy = mouse_get_y();
    int item_idx = 0;
    fe_ctx_sel = -1;
    for (int i = 0; i < 13; i++) {
        if (!in_trash && i == 12) continue;
        const char* lbl = fe_ctx_item_label(i, in_trash);
        if (lbl[0] == 0) { item_idx++; continue; }
        int iy = my + 2 + item_idx * 22;
        int hover = (mmx >= mx && mmx < mx + menu_w && mmy >= iy && mmy < iy + 22);
        if (hover) {
            fe_ctx_sel = i;
            gfx_fill_rect(mx + 2, iy, menu_w - 4, 22, 0x1A437A);
        }
        gfx_draw_string_transparent(mx + 10, iy + 3, lbl, hover ? 0xFFFFFF : 0xC9D1D9);
        item_idx++;
    }
}

static void fe_draw_props_overlay(int x, int y, int w, int h) {
    if (!fe_props_visible || fe_selected_idx < 0) return;
    int pw = 340, ph = 260;
    int px = x + (w - pw) / 2;
    int py = y + (h - ph) / 2;
    int mmx = mouse_get_x(), mmy = mouse_get_y();

    gfx_fill_rect(px, py, pw, ph, 0x1D1F26);
    gfx_draw_rect_outline(px, py, pw, ph, 1, 0x6B6B6B);

    gfx_fill_rect(px + 2, py + 2, pw - 4, 28, 0x252526);
    gfx_draw_string_transparent(px + 12, py + 8, "Properties", 0xCCCCCC);

    int cx_close = px + pw - 22;
    int cy_close = py + 4;
    int chov = (mmx >= cx_close && mmx < cx_close+18 && mmy >= cy_close && mmy < cy_close+18);
    if (chov) gfx_fill_rect(cx_close, cy_close, 18, 18, 0x555555);
    gfx_draw_line(cx_close+5, cy_close+5, cx_close+13, cy_close+13, chov?0xFFFFFF:0x8B949E);
    gfx_draw_line(cx_close+13, cy_close+5, cx_close+5, cy_close+13, chov?0xFFFFFF:0x8B949E);

    gfx_draw_hline(px + 6, py + 32, pw - 12, 0x30363D);

    const fe_file_entry_t* e = &fe_files[fe_selected_idx];
    int is_dir = (e->type == 1);

    // Icon
    fe_draw_icon(px + 12, py + 40, e->name, is_dir);
    gfx_draw_string_transparent(px + 36, py + 44, e->name, 0xE4E6EA);

    // Section
    int ly = py + 68;
    gfx_draw_string_transparent(px + 8, ly, "General", 0x8B949E);
    gfx_draw_hline(px + 8, ly + 14, pw - 16, 0x30363D);
    ly += 22;

    int lx = px + 12;
    int lw = 80;
    int lx2 = lx + lw;

    gfx_draw_string_transparent(lx, ly, "Type:", 0x8B949E);
    const char* tn = fe_get_type_name(e->name, is_dir);
    gfx_draw_string_transparent(lx2, ly, tn, 0xE4E6EA);
    ly += 18;

    gfx_draw_string_transparent(lx, ly, "Size:", 0x8B949E);
    char sz[24]; format_size(e->size, sz);
    gfx_draw_string_transparent(lx2, ly, sz, 0xE4E6EA);
    ly += 18;

    gfx_draw_string_transparent(lx, ly, "Location:", 0x8B949E);
    char loc[64]; fs_pwd(loc, 64);
    gfx_draw_string_transparent(lx2, ly, loc, 0xE4E6EA);
    ly += 18;

    char time_str[32]; fs_format_time(e->mod_time, time_str, 32);
    gfx_draw_string_transparent(lx, ly, "Modified:", 0x8B949E);
    gfx_draw_string_transparent(lx2, ly, time_str, 0xE4E6EA);
    ly += 18;

    // Flags section
    ly += 4;
    gfx_draw_string_transparent(px + 8, ly, "Permissions", 0x8B949E);
    gfx_draw_hline(px + 8, ly + 14, pw - 16, 0x30363D);
    ly += 22;

    char perm_str[16];
    fs_get_permission_string(e->flags, perm_str);
    gfx_draw_string_transparent(lx, ly, "Mode:", 0x8B949E);
    gfx_draw_string_transparent(lx2, ly, perm_str, 0xE4E6EA);
    ly += 18;

    char flag_str[32]; int fi = 0;
    if (e->flags & FS_FLAG_READONLY) { const char* s = "Read-only "; while(*s && fi<30) flag_str[fi++]=*s++; }
    if (e->flags & FS_FLAG_HIDDEN) { const char* s = "Hidden "; while(*s && fi<30) flag_str[fi++]=*s++; }
    if (e->flags & FS_FLAG_SYSTEM) { const char* s = "System "; while(*s && fi<30) flag_str[fi++]=*s++; }
    if (e->flags & FS_FLAG_EXECUTABLE) { const char* s = "Executable "; while(*s && fi<30) flag_str[fi++]=*s++; }
    if (!fi) { const char* s = "None"; while(*s && fi<30) flag_str[fi++]=*s++; }
    flag_str[fi] = 0;
    gfx_draw_string_transparent(lx, ly, "Flags:", 0x8B949E);
    gfx_draw_string_transparent(lx2, ly, flag_str, 0xE4E6EA);

    // Close button
    int bbx = px + pw - 72, bby = py + ph - 26;
    int bbh = 20, bbw = 60;
    int btn_hover = (mmx >= bbx && mmx < bbx + bbw && mmy >= bby && mmy < bby + bbh);
    gfx_fill_rect(bbx, bby, bbw, bbh, btn_hover ? 0x333333 : 0x161B22);
    gfx_draw_rect_outline(bbx, bby, bbw, bbh, 1, 0x555555);
    int tll = gfx_strlen("Close") * 8;
    gfx_draw_string_transparent(bbx + (bbw - tll) / 2, bby + 3, "Close", 0xCCCCCC);
}

static void fe_draw_rename_overlay(int x, int y, int w, int h) {
    if (!fe_renaming || fe_selected_idx < 0) return;
    int bx = x + (w - 260) / 2;
    int by = y + h - 60;
    gfx_fill_rect(bx, by, 260, 28, 0x1D1F26);
    gfx_draw_rect_outline(bx, by, 260, 28, 1, 0x58A6FF);
    char prompt[72] = "Rename: ";
    int pi = 8;
    for (int j = 0; fe_rename_buf[j] && pi < 70; j++)
        prompt[pi++] = fe_rename_buf[j];
    prompt[pi] = 0;
    gfx_draw_string_transparent(bx + 6, by + 6, prompt, 0xFFFFFF);
}

static void fe_draw_create_overlay(int x, int y, int w, int h) {
    if (!fe_creating) return;
    int bx = x + (w - 260) / 2;
    int by = y + h - 60;
    gfx_fill_rect(bx, by, 260, 28, 0x1D1F26);
    gfx_draw_rect_outline(bx, by, 260, 28, 1, 0x3FB950);
    const char* prefix = (fe_creating == 1) ? "New Folder: " : "New File: ";
    char prompt[72];
    int pi = 0;
    for (int j = 0; prefix[j] && pi < 70; j++) prompt[pi++] = prefix[j];
    for (int j = 0; fe_create_buf[j] && pi < 70; j++)
        prompt[pi++] = fe_create_buf[j];
    prompt[pi] = 0;
    gfx_draw_string_transparent(bx + 6, by + 6, prompt, 0xFFFFFF);
}

// ── Mouse Handling ──────────────────────────────────────────────

static void fe_handle_mouse(int x, int y, int w, int h, int visible_rows) {
    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int left_clk = (mbtn & MOUSE_LEFT) && !(fe_prev_mouse & MOUSE_LEFT);
    int right_clk = (mbtn & MOUSE_RIGHT) && !(fe_prev_mouse & MOUSE_RIGHT);
    fe_prev_mouse = mbtn;

    if (!left_clk && !right_clk) return;

    int body_h = h - CONTENT_TOP - STATUSBAR_H;
    if (body_h < 20) body_h = 20;
    int main_x = x + SIDEBAR_W;
    int main_w = w - SIDEBAR_W;
    int main_y = y + CONTENT_TOP;
    int col_hdr_h = 22;
    int list_y = main_y + col_hdr_h;
    int in_toolbar = (mx >= x && mx < x + w && my >= y && my < y + TOOLBAR_H);
    // Toolbar buttons are at fixed positions
    int bx = x + 6;
    int by = y + 2;
    int bw = 28, gap = 4;
    int in_sidebar = (mx >= x && mx < x + SIDEBAR_W && my >= y + CONTENT_TOP && my < y + CONTENT_TOP + body_h);
    int in_header = (mx >= main_x && mx < main_x + main_w && my >= main_y && my < main_y + col_hdr_h);
    int in_list = (mx >= main_x && mx < main_x + main_w && my >= list_y && my < list_y + body_h - col_hdr_h);

    // ── Left click: dismiss context menu / properties ──
    if (left_clk) {
        if (fe_props_visible) {
            int pw = 340, ph = 260;
            int px = x + (w - pw) / 2;
            int py = y + (h - ph) / 2;
            int close_x = px + pw - 22, close_y = py + 4;
            int bbx = px + pw - 72, bby = py + ph - 26;
            if ((mx >= close_x && mx < close_x + 18 && my >= close_y && my < close_y + 18) ||
                (mx >= bbx && mx < bbx + 60 && my >= bby && my < bby + 20))
                fe_props_visible = 0;
            else if (mx < px || mx > px + pw || my < py || my > py + ph)
                fe_props_visible = 0;
            return;
        }
        if (fe_ctx_visible) {
            int in_trash = fe_is_in_trash();
            int item_count = in_trash ? 13 : 12;
            int menu_w = 170, menu_h = item_count * 22 + 4;
            int cmx = fe_ctx_x, cmy = fe_ctx_y;
            int fw = get_fb_width(), fh = get_fb_height();
            if (cmx + menu_w > fw) cmx = fw - menu_w - 4;
            if (cmy + menu_h > fh) cmy = fh - menu_h - 4;
            if (cmx < 0) cmx = 0;
            if (cmy < 0) cmy = 0;
            int on_menu = (mx >= cmx && mx < cmx + menu_w && my >= cmy && my < cmy + menu_h);
            if (on_menu && fe_ctx_sel >= 0) {
                fe_ctx_execute(fe_ctx_sel, in_trash);
                return;
            }
            if (!on_menu) { fe_ctx_hide(); return; }
            return;
        }
    }

    // ── Right click on list → context menu ──
    if (right_clk && in_list) {
        int row = (my - list_y) / ROW_HEIGHT;
        int idx = fe_scroll_offset + row;
        if (idx >= 0 && idx < fe_file_count) fe_selected_idx = idx;
        fe_ctx_show(mx, my);
        return;
    }

    if (!left_clk) return;

    // ── Left click on toolbar ──
    if (in_toolbar) {
        int by = y + 2;
        int bw = 28, gap = 4;
        int bx0 = x + 6;
        for (int i = 0; i < 6; i++) {
            int bx = bx0 + i * (bw + gap);
            int disabled = 0;
            if (i == 0 && fe_history_pos <= 0) disabled = 1;
            if (i == 1 && fe_history_pos >= fe_history_count - 1) disabled = 1;
            if (!disabled && mx >= bx && mx < bx + bw && my >= by && my < by + 28) {
                if (i == 0 && fe_history_pos > 0) {
                    fe_history_pos--;
                    fe_navigate_to_path(fe_history_paths[fe_history_pos]);
                } else if (i == 1 && fe_history_pos < fe_history_count - 1) {
                    fe_history_pos++;
                    fe_navigate_to_path(fe_history_paths[fe_history_pos]);
                } else if (i == 2) {
                    fe_change_dir("..");
                } else if (i == 3) {
                    fe_refresh();
                } else if (i == 4) {
                    fe_go_home();
                } else if (i == 5) {
                    fe_go_root();
                }
                return;
            }
        }
        return;
    }

    // ── Left click on sidebar ──
    if (in_sidebar) {
        int sy = y + CONTENT_TOP;
        if (my >= sy + 28) {
            int idx = (my - sy - 28) / 26;
            if (idx >= 0 && idx < PLACE_COUNT) {
                fe_go_place(idx);
                return;
            }
        }
        return;
    }

    // ── Left click on column headers → sort ──
    if (in_header) {
        int rel_x = mx - main_x - 4;
        int col_w_local[5];
        col_w_local[0] = main_w * 34 / 100; if (col_w_local[0] < 100) col_w_local[0] = 100;
        col_w_local[1] = main_w * 12 / 100; if (col_w_local[1] < 55) col_w_local[1] = 55;
        col_w_local[2] = main_w * 20 / 100; if (col_w_local[2] < 70) col_w_local[2] = 70;
        col_w_local[3] = main_w * 20 / 100; if (col_w_local[3] < 80) col_w_local[3] = 80;
        col_w_local[4] = main_w - col_w_local[0] - col_w_local[1] - col_w_local[2] - col_w_local[3] - 8;
        int cx = 0;
        for (int c = 0; c < 5; c++) {
            cx += col_w_local[c];
            if (rel_x < cx) {
                if (c == (int)fe_sort_col)
                    fe_sort_asc = !fe_sort_asc;
                else {
                    fe_sort_col = (fe_sort_col_t)c;
                    fe_sort_asc = 1;
                }
                fe_sort_files();
                return;
            }
        }
        return;
    }

    // ── Left click on file list ──
    if (in_list) {
        int row = (my - list_y) / ROW_HEIGHT;
        int idx = fe_scroll_offset + row;
        if (idx >= 0 && idx < fe_file_count) {
            if (idx != fe_selected_idx) fe_disarm_delete();
            uint32_t now = timer_get_ms();
            int is_double = (idx == fe_last_click_idx && now - fe_last_click_time < 400);
            fe_last_click_time = now;
            fe_last_click_idx = idx;
            if (is_double) {
                fe_open_selected();
            } else {
                fe_selected_idx = idx;
            }
        }
        return;
    }
}

// ── Callbacks ───────────────────────────────────────────────────

static void fe_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy; (void)id;

    gfx_fill_rect(x, y, w, h, 0x0D1117);

    int body_h = h - CONTENT_TOP - STATUSBAR_H;
    if (body_h < 20) body_h = 20;
    int main_x = x + SIDEBAR_W;
    int main_w = w - SIDEBAR_W;
    if (main_w < 100) main_w = 100;

    fe_draw_toolbar(x, y, w);
    fe_draw_pathbar(x, y + TOOLBAR_H, w);
    fe_draw_sidebar(x, y + CONTENT_TOP, body_h);
    int visible_rows = fe_draw_mainview(main_x, y + CONTENT_TOP, main_w, body_h);
    fe_check_disarm_auto();

    fe_draw_statusbar(x, y + h - STATUSBAR_H, w);
    fe_draw_context_menu();
    fe_draw_props_overlay(x, y, w, h);
    fe_draw_rename_overlay(x, y, w, h);

    fe_handle_mouse(x, y, w, h, visible_rows);
}

static void fe_on_key(int id, char key_in) {
    (void)id;
    unsigned char k = (unsigned char)key_in;

    if (fe_renaming) {
        if (k == 27) { fe_renaming = 0; return; }
        if (k == '\b' && fe_rename_len > 0) fe_rename_buf[--fe_rename_len] = 0;
        else if (k == '\n' && fe_rename_len > 0) { fe_do_rename(); }
        else if (k >= 32 && k <= 126 && fe_rename_len < 62) {
            fe_rename_buf[fe_rename_len++] = k;
            fe_rename_buf[fe_rename_len] = 0;
        }
        return;
    }

    if (fe_creating) {
        if (k == 27) { fe_creating = 0; return; }
        if (k == '\b' && fe_create_len > 0) fe_create_buf[--fe_create_len] = 0;
        else if (k == '\n' && fe_create_len > 0) { fe_do_create(); }
        else if (k >= 32 && k <= 126 && fe_create_len < 62) {
            fe_create_buf[fe_create_len++] = k;
            fe_create_buf[fe_create_len] = 0;
        }
        return;
    }

    if (k == 27) {
        if (fe_ctx_visible) { fe_ctx_hide(); return; }
        if (fe_props_visible) { fe_props_visible = 0; return; }
    }

    if (k == 3) { fe_copy_selected(); return; }
    if (k == 22) { fe_paste_clipboard(); return; }
    if (k == 24) { fe_cut_selected(); return; }
    if (k == 4) { fe_trash_selected(); return; }

    if (k == '\n') {
        if (fe_ctx_visible && fe_ctx_sel >= 0) {
            fe_ctx_execute(fe_ctx_sel, fe_is_in_trash());
        } else {
            fe_open_selected();
        }
        return;
    }

    if (k == '\b') {
        fe_change_dir("..");
        return;
    }

    if (k == KEY_UP) {
        if (fe_selected_idx > 0) fe_selected_idx--;
        if (fe_selected_idx < fe_scroll_offset) fe_scroll_offset = fe_selected_idx;
        return;
    }
    if (k == KEY_DOWN) {
        if (fe_selected_idx < fe_file_count - 1) fe_selected_idx++;
        int vis_rows = 10;
        wm_window_t* win = wm_get_window(fe_win_id);
        if (win) {
            int bb = win->h - WM_TITLEBAR_H - CONTENT_TOP - STATUSBAR_H - 22;
            vis_rows = bb / ROW_HEIGHT;
        }
        if (vis_rows < 1) vis_rows = 1;
        if (fe_selected_idx >= fe_scroll_offset + vis_rows)
            fe_scroll_offset = fe_selected_idx - vis_rows + 1;
        return;
    }
    if (k == KEY_PAGE_UP) {
        fe_scroll_offset -= 10;
        if (fe_scroll_offset < 0) fe_scroll_offset = 0;
        if (fe_selected_idx >= 0) fe_selected_idx = fe_scroll_offset;
        return;
    }
    if (k == KEY_PAGE_DOWN) {
        fe_scroll_offset += 10;
        if (fe_scroll_offset > fe_file_count - 1) fe_scroll_offset = fe_file_count - 1;
        if (fe_scroll_offset < 0) fe_scroll_offset = 0;
        if (fe_selected_idx >= 0) fe_selected_idx = fe_scroll_offset;
        return;
    }
}

static void fe_on_resize(int win_id, int w, int h) {
    (void)win_id; (void)w; (void)h;
}

void file_explorer(void) {
    fe_file_count = 0;
    fe_selected_idx = -1;
    fe_scroll_offset = 0;
    fe_ctx_visible = 0;
    fe_renaming = 0;
    fe_creating = 0;
    fe_clipboard_has = 0;
    fe_props_visible = 0;
    fe_prev_mouse = 0;
    fe_hover_row = -1;
    fe_sort_col = SORT_NAME;
    fe_sort_asc = 1;
    fe_history_pos = -1;
    fe_last_click_time = 0;
    fe_last_click_idx = -1;

    fe_go_home();

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 820, win_h = 540;
    if (win_w > (int)fw) win_w = fw;
    if (win_h > (int)fh) win_h = fh;
    fe_win_id = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h,
                                "File Explorer", 0x58A6FF, fe_on_render, fe_on_key, fe_on_resize);
}

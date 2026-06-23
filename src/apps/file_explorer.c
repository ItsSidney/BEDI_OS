// ============================================================
//  BEDI OS — Premium File Explorer (Dynamic Edition)
//  Supports dynamic listing, details panel, renaming,
//  deletion, creation, and scrollable list view.
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/input/mouse.h"
#include "gui/wm.h"
#include "gui/gui.h"
#include "gui/icons.h"
#include "filesystem/filesystem.h"
#include "commands/commands.h"
#include "apps/text_editor.h"
#include "apps/imgview.h"

#define MAX_FE_FILES 128
#define ROW_HEIGHT   26

static char fe_file_names[MAX_FE_FILES][64];
static int fe_file_sizes[MAX_FE_FILES];
static int fe_file_types[MAX_FE_FILES];
static uint32_t fe_file_mod_times[MAX_FE_FILES];
static int fe_file_count = 0;

static int fe_win_id = -1;
static int fe_selected_idx = -1;
static int fe_scroll_offset = 0;
static int fe_prev_mouse = 0;

static char fe_input_buffer[64];
static int fe_input_len = 0;

// Helper to format file sizes
static void format_size(int bytes, char* buf) {
    if (bytes < 1024) {
        itoa(bytes, buf);
        int len = strlen(buf);
        buf[len++] = ' ';
        buf[len++] = 'B';
        buf[len] = 0;
    } else {
        int kb = bytes / 1024;
        itoa(kb, buf);
        int len = strlen(buf);
        buf[len++] = ' ';
        buf[len++] = 'K';
        buf[len++] = 'B';
        buf[len] = 0;
    }
}

// Helper to check extensions
static int ends_with(const char* name, const char* ext) {
    int nl = strlen(name);
    int el = strlen(ext);
    if (nl < el) return 0;
    return (strcmp(name + nl - el, ext) == 0);
}

static int is_txt_file(const char* name) { return ends_with(name, ".txt"); }
static int is_bc_file(const char* name) { return ends_with(name, ".bc"); }
static int is_bin_file(const char* name) { return ends_with(name, ".bin"); }
static int is_bmp_file(const char* name) {
    return ends_with(name, ".bmp") || ends_with(name, ".BMP");
}

static const char* fe_get_type_name(const char* name, int is_dir) {
    if (is_dir) return "Folder";
    if (is_txt_file(name)) return "Text File";
    if (is_bc_file(name)) return "Bedi-C Source";
    if (is_bin_file(name)) return "Executable";
    if (is_bmp_file(name)) return "Bitmap Image";
    return "Binary";
}

// Load files from the current directory
static void fe_load_files(void) {
    fe_file_count = 0;
    int curr_dir = fs_get_current_dir();
    
    if (curr_dir != -1) {
        strcpy(fe_file_names[fe_file_count], "..");
        fe_file_sizes[fe_file_count] = 0;
        fe_file_types[fe_file_count] = 1;
        fe_file_mod_times[fe_file_count] = 0;
        fe_file_count++;
    }
    
    int user_dir = get_user_dir();
    if (curr_dir == user_dir) {
        strcpy(fe_file_names[fe_file_count], "ROOT");
        fe_file_sizes[fe_file_count] = 0;
        fe_file_types[fe_file_count] = 1;
        fe_file_mod_times[fe_file_count] = 0;
        fe_file_count++;
    }
    
    char name[64];
    int size, type, parent;
    uint8_t flags;
    uint32_t mod_time;
    for (int i = 0; i < 256 && fe_file_count < MAX_FE_FILES; i++) {
        if (fs_get_node(i, name, &size, &type, &parent, &flags, &mod_time) == 0) {
            if (parent == curr_dir) {
                strcpy(fe_file_names[fe_file_count], name);
                fe_file_sizes[fe_file_count] = size;
                fe_file_types[fe_file_count] = type;
                fe_file_mod_times[fe_file_count] = mod_time;
                fe_file_count++;
            }
        }
    }
}

static void fe_change_dir(const char* name) {
    if (strcmp(name, "..") == 0) {
        fs_cd("..");
    } else if (strcmp(name, "ROOT") == 0) {
        fs_cd("/");
    } else {
        fs_cd(name);
    }
    fe_load_files();
    fe_selected_idx = -1;
    fe_scroll_offset = 0;
}

static void fe_on_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)vx; (void)vy; (void)id;
    personalization_t* p = get_personalization();
    uint32_t text_clr = (p->theme == 0) ? 0xFFFFFF : 0x000000;
    uint32_t accent = get_accent_color();

    // 1. Toolbar Background
    gfx_fill_rect(x, y, w, 40, (p->theme == 0 ? 0x161B22 : 0xEEEEEE));
    gfx_draw_hline(x, y + 40, w, (p->theme == 0 ? 0x30363D : 0xCCCCCC));
    
    // Path display
    char path[128];
    fs_pwd(path, 128);
    if (w > 500) {
        int pl = strlen(path);
        if (pl > 40) gfx_draw_string_transparent(x + w - 340, y + 12, "...", 0x58A6FF);
        gfx_draw_string_transparent(x + w - 320, y + 12, pl > 40 ? path + pl - 40 : path, 0x58A6FF);
    }
    
    // 2. Middle Content Panel
    int list_w = w * 2 / 3;
    if (list_w > 465) list_w = 465;

    int body_h = h - 81; 
    int visible_rows = body_h / ROW_HEIGHT;
    if (visible_rows > 20) visible_rows = 20;

    gfx_fill_rect(x, y + 41, list_w, body_h, 0x0D1117);
    gfx_draw_vline(x + list_w, y + 41, body_h, 0x30363D);
    gfx_fill_rect(x + list_w + 1, y + 41, w - list_w - 1, body_h, 0x0A0E14);
    
    // Draw List Columns Header
    gfx_fill_rect(x, y + 41, list_w, 24, 0x161B22);
    gfx_draw_hline(x, y + 65, list_w, 0x30363D);
    gfx_draw_string_transparent(x + 45, y + 45, "Name", 0x8B949E);
    if (list_w > 300) {
        gfx_draw_string_transparent(x + list_w - 220, y + 45, "Type", 0x8B949E);
        gfx_draw_string_transparent(x + list_w - 100, y + 45, "Size", 0x8B949E);
    }
    
    // Draw File Rows
    for (int r = 0; r < visible_rows; r++) {
        int idx = fe_scroll_offset + r;
        if (idx >= fe_file_count) break;
        
        int row_y = y + 66 + r * ROW_HEIGHT;
        int is_selected = (fe_selected_idx == idx);
        
        if (is_selected) {
            gfx_fill_rect(x + 5, row_y + 1, list_w - 10, ROW_HEIGHT - 2, 0x1F2937);
            gfx_draw_rect_outline(x + 5, row_y + 1, list_w - 10, ROW_HEIGHT - 2, 1, 0x58A6FF);
        }
        
        // Render Row Icon
        int is_dir = (fe_file_types[idx] == 1);
        if (is_dir) {
            gfx_fill_rect(x + 15, row_y + 6, 16, 12, 0xEBCB8B);
            gfx_fill_rect(x + 15, row_y + 4, 6, 2, 0xEBCB8B);
        } else {
            const char* fname = fe_file_names[idx];
            if (is_txt_file(fname)) {
                gfx_fill_rect(x + 17, row_y + 4, 12, 16, 0xECEFF4);
                gfx_draw_rect_outline(x + 17, row_y + 4, 12, 16, 1, 0x4C566A);
            } else if (is_bc_file(fname)) {
                gfx_fill_rect(x + 17, row_y + 4, 12, 16, 0xBC8CFF); // Purple for Bedi-C
                gfx_draw_rect_outline(x + 17, row_y + 4, 12, 16, 1, 0xD2A8FF);
                gfx_draw_string_transparent(x + 19, row_y + 5, "B", 0xFFFFFF);
            } else if (is_bin_file(fname)) {
                gfx_fill_rect(x + 17, row_y + 4, 12, 16, 0x238636); // Green for binaries
                gfx_draw_rect_outline(x + 17, row_y + 4, 12, 16, 1, 0x3FB950);
                gfx_draw_string_transparent(x + 19, row_y + 5, ">", 0xFFFFFF);
            } else if (is_bmp_file(fname)) {
                gfx_fill_rect(x + 17, row_y + 4, 12, 16, 0x1D1F26);
                gfx_draw_rect_outline(x + 17, row_y + 4, 12, 16, 1, 0x58A6FF);
                gfx_fill_circle(x + 21, row_y + 7, 2, 0xFBBF24);
                gfx_fill_rect(x + 18, row_y + 15, 10, 3, 0x22D3EE);
                gfx_fill_rect(x + 22, row_y + 12, 3, 4, 0x4D5059);
            } else {
                gfx_fill_rect(x + 17, row_y + 4, 12, 16, 0x161B22);
                gfx_draw_rect_outline(x + 17, row_y + 4, 12, 16, 1, 0x3FB950);
                gfx_draw_string_transparent(x + 19, row_y + 5, "1", 0x3FB950);
            }
        }
        
        // Render Row Text
        char name_disp[24];
        int nl = strlen(fe_file_names[idx]);
        if (nl > 20) {
            for (int i = 0; i < 17; i++) name_disp[i] = fe_file_names[idx][i];
            name_disp[17] = '.'; name_disp[18] = '.'; name_disp[19] = '.'; name_disp[20] = 0;
        } else {
            strcpy(name_disp, fe_file_names[idx]);
        }
        
        uint32_t row_clr = is_selected ? 0xF0F6FC : (is_dir ? 0x8B949E : 0xC9D1D9);
        gfx_draw_string_transparent(x + 45, row_y + 5, name_disp, row_clr);
        
        if (list_w > 300) {
            const char* type_name = fe_get_type_name(fe_file_names[idx], is_dir);
            gfx_draw_string_transparent(x + list_w - 220, row_y + 5, type_name, 0x6E7681);
            if (is_dir) {
                gfx_draw_string_transparent(x + list_w - 100, row_y + 5, "--", 0x6E7681);
            } else {
                char size_str[16];
                format_size(fe_file_sizes[idx], size_str);
                gfx_draw_string_transparent(x + list_w - 100, row_y + 5, size_str, 0x8B949E);
            }
        }
    }
    
    // Details Panel
    if (w - list_w > 100) {
        if (fe_selected_idx >= 0 && fe_selected_idx < fe_file_count) {
             gfx_draw_string_transparent(x + list_w + 10, y + 60, "DETAILS", accent);
             gfx_draw_string_transparent(x + list_w + 10, y + 80, fe_file_names[fe_selected_idx], text_clr);
        }
    }

    // Handle row clicks manually (no WM buttons to avoid black fill overlay)
    int mbtn = mouse_get_buttons();
    int mx = mouse_get_x(), my = mouse_get_y();
    int clicked = (mbtn & 1) && !(fe_prev_mouse & 1);
    fe_prev_mouse = mbtn;
    if (clicked) {
        int rel_x = mx - x;
        int rel_y = my - (y + 66);
        if (rel_x >= 0 && rel_x < list_w && rel_y >= 0) {
            int row = rel_y / ROW_HEIGHT;
            int idx = fe_scroll_offset + row;
            if (idx >= 0 && idx < fe_file_count) {
                if (fe_selected_idx == idx) {
                    fe_change_dir(fe_file_names[idx]);
                } else {
                    fe_selected_idx = idx;
                    strcpy(fe_input_buffer, fe_file_names[idx]);
                    fe_input_len = strlen(fe_input_buffer);
                }
            }
        }
    }

    // 3. Bottom Bar
    int bottom_y = y + h - 40;
    gfx_fill_rect(x, bottom_y, w, 40, 0x161B22);
    gfx_draw_hline(x, bottom_y, w, 0x30363D);
    gfx_draw_string_transparent(x + 15, bottom_y + 12, "Name:", 0x8B949E);
    int input_box_x = x + 60;
    int input_box_w = (w > 400) ? 240 : (w - 180);
    if (input_box_w < 50) input_box_w = 50;
    gfx_fill_rect(input_box_x, bottom_y + 8, input_box_w, 24, 0x0D1117);
    gfx_draw_rect_outline(input_box_x, bottom_y + 8, input_box_w, 24, 1, 0x30363D);
    gfx_draw_string_transparent(input_box_x + 8, bottom_y + 12, fe_input_buffer, 0xF0F6FC);
}

static void fe_on_click(int win_id, int btn_id) {
    if (btn_id == 100) { fe_change_dir(".."); }
    else if (btn_id == 101) { if (fe_input_len > 0) { fs_create(fe_input_buffer); fe_load_files(); } }
    else if (btn_id == 102) { if (fe_input_len > 0) { fs_mkdir(fe_input_buffer); fe_load_files(); } }
    else if (btn_id == 103) { 
        if (fe_selected_idx >= 0) { 
            char n[64]; strcpy(n, fe_file_names[fe_selected_idx]); 
            if (fe_file_types[fe_selected_idx] == 1) fe_change_dir(n); 
            else if (is_bmp_file(n)) {
                imgview_open(n);
            } else if (is_bin_file(n)) {
                extern void brun_main(char*);
                brun_main(n);
            } else if (is_bc_file(n) || is_txt_file(n)) {
                text_editor_open(n);
            }
        } 
    }
    else if (btn_id == 104) { if (fe_selected_idx >= 0 && fe_input_len > 0) { fs_rename(fe_file_names[fe_selected_idx], fe_input_buffer); fe_load_files(); } }
    else if (btn_id == 105) { if (fe_selected_idx >= 0) { fs_delete(fe_file_names[fe_selected_idx]); fe_load_files(); fe_selected_idx = -1; } }
}

static void fe_on_key(int id, char key) {
    (void)id;
    if (key == '\b') { if (fe_input_len > 0) fe_input_buffer[--fe_input_len] = 0; }
    else if (key >= 32 && key <= 126) { if (fe_input_len < 63) { fe_input_buffer[fe_input_len++] = key; fe_input_buffer[fe_input_len] = 0; } }
}

static void fe_on_resize(int win_id, int w, int h) {
    wm_clear_buttons(win_id);
    int bh = h - WM_TITLEBAR_H;
    wm_add_button(win_id, 100, 10, 8, 70, 24, "Back", 0x21262D, 0xC9D1D9, fe_on_click);
    wm_add_button(win_id, 103, 90, 8, 70, 24, "Open", 0x21262D, 0xC9D1D9, fe_on_click);
    wm_add_button(win_id, 104, 170, 8, 70, 24, "Rename", 0x21262D, 0xC9D1D9, fe_on_click);
    wm_add_button(win_id, 105, 250, 8, 70, 24, "Delete", 0x21262D, 0xC9D1D9, fe_on_click);
    wm_add_button(win_id, 101, w - 180, bh - 32, 80, 24, "+ File", 0x21262D, 0xC9D1D9, fe_on_click);
    wm_add_button(win_id, 102, w - 90, bh - 32, 80, 24, "+ Folder", 0x21262D, 0xC9D1D9, fe_on_click);
}

void file_explorer(void) {
    fe_load_files();
    fe_selected_idx = -1; fe_scroll_offset = 0; fe_input_buffer[0] = 0; fe_input_len = 0;
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 700, win_h = 500;
    fe_win_id = wm_open_window((fw-win_w)/2, (fh-win_h)/2, win_w, win_h, "File Explorer", 0x58A6FF, fe_on_render, fe_on_key, fe_on_resize);
    fe_on_resize(fe_win_id, win_w, win_h);
}

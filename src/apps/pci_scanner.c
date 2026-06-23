// ============================================================
//  BEDI OS — Professional PCI Hardware Scanner (PRO Edition)
// ============================================================
#include "drivers/bus/pci.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/gui.h"

#define MAX_ITEMS_PER_PAGE 12

static int pci_page = 0;

static void draw_pci_logo(int x, int y, int size) {
    uint32_t gold = 0xD29922;
    uint32_t board = 0x1B1B1B;
    uint32_t trace = 0x333333;
    gfx_draw_bevel_rect(x, y, size, size, board, 0);
    for (int i = 0; i < 8; i++) gfx_fill_rect(x + 4 + i * 4, y + size - 8, 2, 6, gold);
    gfx_draw_bevel_rect(x + size/4, y + size/4, size/2, size/2, 0x222222, 1);
    gfx_draw_rect_outline(x + size/4, y + size/4, size/2, size/2, 1, trace);
}

static void btn_pci_prev(int win_id, int btn_id) {
    if (pci_page > 0) pci_page--;
}

static void btn_pci_next(int win_id, int btn_id) {
    int total = pci_get_device_count();
    int max_pages = (total + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    if (pci_page < max_pages - 1) pci_page++;
}

static void pci_scanner_render(int id, int x, int y, int w, int h, int vx, int vy) {
    personalization_t* p = get_personalization();
    uint32_t bg = (p->theme == 0) ? 0x0D1117 : 0xF0F6FC;
    uint32_t text_clr = (p->theme == 0) ? 0xFFFFFF : 0x000000;
    uint32_t accent = get_accent_color();
    uint32_t head_bg = (p->theme == 0) ? 0x161B22 : 0xEEEEEE;

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_bevel_rect(x + 8, y + 8, w - 16, 60, head_bg, 1);
    
    draw_pci_logo(x + 20, y + 14, 48);
    gfx_draw_string_transparent(x + 80, y + 22, "PCI HARDWARE INVENTORY", accent);
    gfx_draw_string_transparent(x + 80, y + 42, "Subsystem Analysis & Bus Discovery", 0x8B949E);
    
    int ty = y + 80;
    gfx_fill_rect(x + 10, ty, w - 20, 25, (p->theme == 0 ? 0x21262D : 0xDDDDDD));
    
    // Dynamic column offsets
    int col1 = 20;
    int col2 = 90;
    int col3 = (w > 400) ? 210 : 180;

    gfx_draw_string_transparent(x + col1, ty + 6, "ADDR", accent);
    gfx_draw_string_transparent(x + col2, ty + 6, "VENDOR:DEVICE", accent);
    if (w > 300) gfx_draw_string_transparent(x + col3, ty + 6, "CLASS DESCRIPTION", accent);
    
    int total_devices = pci_get_device_count();
    int start_idx = pci_page * MAX_ITEMS_PER_PAGE;
    int end_idx = start_idx + MAX_ITEMS_PER_PAGE;
    if (end_idx > total_devices) end_idx = total_devices;
    
    int cy = ty + 35;
    for (int idx = start_idx; idx < end_idx; idx++) {
        if (cy + 20 > y + h - 40) break; // Don't draw over footer
        
        pci_device_t* dev = pci_get_device(idx);
        if (!dev) continue;
        char bsf[16] = "00:00.0";
        bsf[0] = (dev->bus / 10) + '0'; bsf[1] = (dev->bus % 10) + '0';
        bsf[3] = (dev->slot / 10) + '0'; bsf[4] = (dev->slot % 10) + '0';
        bsf[6] = (dev->func % 10) + '0';
        gfx_draw_string_transparent(x + col1, cy, bsf, 0x8B949E);
        char vid[16] = "0000:0000";
        const char hex[] = "0123456789ABCDEF";
        vid[0]=hex[(dev->vendor_id>>12)&0xF]; vid[1]=hex[(dev->vendor_id>>8)&0xF]; vid[2]=hex[(dev->vendor_id>>4)&0xF]; vid[3]=hex[dev->vendor_id&0xF];
        vid[5]=hex[(dev->device_id>>12)&0xF]; vid[6]=hex[(dev->device_id>>8)&0xF]; vid[7]=hex[(dev->device_id>>4)&0xF]; vid[8]=hex[dev->device_id&0xF];
        gfx_draw_string_transparent(x + col2, cy, vid, (p->theme == 0 ? 0xF0883E : 0xAF5800));
        
        if (w > 300) {
            const char* cls = pci_get_class_name(dev->class_id);
            char cls_disp[32];
            int cl = gfx_strlen(cls);
            int max_cl = (w - col3 - 20) / 8;
            if (cl > max_cl && max_cl > 3) {
                for(int i=0; i<max_cl-3; i++) cls_disp[i] = cls[i];
                cls_disp[max_cl-3] = '.'; cls_disp[max_cl-2] = '.'; cls_disp[max_cl-1] = '.'; cls_disp[max_cl] = 0;
                gfx_draw_string_transparent(x + col3, cy, cls_disp, text_clr);
            } else {
                gfx_draw_string_transparent(x + col3, cy, cls, text_clr);
            }
        }
        
        cy += 20;
        if (idx < end_idx - 1) gfx_draw_hline(x + 15, cy - 4, w - 30, (p->theme == 0 ? 0x21262D : 0xEEEEEE));
    }
    
    int fy = y + h - 35;
    gfx_draw_hline(x + 10, fy - 5, w - 20, (p->theme == 0 ? 0x30363D : 0xCCCCCC));
    char pg[16] = "PAGE 0/0";
    pg[5] = (pci_page + 1) + '0';
    int mp = (total_devices + MAX_ITEMS_PER_PAGE - 1) / MAX_ITEMS_PER_PAGE;
    if (mp == 0) mp = 1;
    pg[7] = mp + '0';
    gfx_draw_string_transparent(x + w/2 - 30, fy + 5, pg, 0x8B949E);
}

static void pci_on_resize(int win_id, int w, int h) {
    wm_clear_buttons(win_id);
    int body_h = h - WM_TITLEBAR_H;
    int btn_y = body_h - 32; // Standard bottom alignment
    wm_add_button(win_id, 1, 15, btn_y, 80, 25, "PREV", 0x30363D, 0xFFFFFF, btn_pci_prev);
    wm_add_button(win_id, 2, w - 95, btn_y, 80, 25, "NEXT", 0x30363D, 0xFFFFFF, btn_pci_next);
}

void pci_scanner_app(void) {
    pci_page = 0;
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 500, win_h = 420;
    int win = wm_open_window((fw-win_w)/2, (fh-win_h)/2, win_w, win_h, "PCI Professional Inventory",
                             get_accent_color(), pci_scanner_render, 0, pci_on_resize);
    pci_on_resize(win, win_w, win_h);
}

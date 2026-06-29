// ============================================================
//  BEDI OS — Modern PCI Hardware Inventory (PRO Edition)
// ============================================================
#include "drivers/bus/pci.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "gui/gui.h"
#include "drivers/input/mouse.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int pci_scroll = 0;
static int hover_idx = -1;
static wm_window_t* last_pci_win = 0;

static void pci_scanner_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx;
    uint32_t bg = 0x0D1117;
    uint32_t card_bg = 0x161B22;
    uint32_t text = 0xE6EDF3;
    uint32_t accent = get_accent_color();
    uint32_t dim = 0x8B949E;
    uint32_t border = 0x30363D;
    uint32_t header_bg = 0x010409;

    gfx_fill_rect(x, y, w, h, bg);

    int mx = mouse_get_x(), my = mouse_get_y();
    int rel_mx = mx - x, rel_my = my - y;

    /* Header */
    gfx_fill_rect(x, y, w, 52, header_bg);
    gfx_draw_string_transparent(x + 16, y + 12, "PCI HARDWARE INVENTORY", accent);
    gfx_draw_string_transparent(x + 16, y + 30, "Complete bus enumeration with device details, BARs, and status", dim);

    int total = pci_get_device_count();
    int item_h = 72;
    int visible_h = h - 52 - 24;
    int max_scroll = total * item_h - visible_h;
    if (max_scroll < 0) max_scroll = 0;
    if (pci_scroll < 0) pci_scroll = 0;
    if (pci_scroll > max_scroll) pci_scroll = max_scroll;

    hover_idx = -1;

    int start_idx = pci_scroll / item_h;
    int end_idx = total;
    int card_x = x + 12;
    int card_w = w - 20;
    int cur_y = y + 52 - (pci_scroll % item_h);

    for (int i = start_idx; i < end_idx; i++) {
        if (cur_y + item_h < y + 52) { cur_y += item_h; continue; }
        if (cur_y + item_h > y + h - 24) break;

        pci_device_t* dev = pci_get_device(i);
        if (!dev) continue;

        int is_hover = (rel_mx >= card_x && rel_mx <= card_x + card_w && rel_my >= cur_y && rel_my <= cur_y + item_h - 4);
        if (is_hover) hover_idx = i;

        /* Card background */
        uint32_t card_fill = is_hover ? 0x1C2128 : card_bg;
        gfx_fill_rect(card_x, cur_y, card_w, item_h - 4, card_fill);
        gfx_draw_rect_outline(card_x, cur_y, card_w, item_h - 4, 1, border);

        int inner_x = card_x + 10;
        int inner_y = cur_y + 6;
        int inner_w = card_w - 18;

        /* Row 1: Address + ID */
        char bsf[16];
        snprintf(bsf, sizeof(bsf), "%02X:%02X.%X", dev->bus, dev->slot, dev->func);
        gfx_draw_string_transparent(inner_x, inner_y, bsf, accent);

        char vidpid[16];
        snprintf(vidpid, sizeof(vidpid), "%04X:%04X", dev->vendor_id, dev->device_id);
        int vp_w = gfx_strlen(vidpid) * 8;
        gfx_draw_string_transparent(inner_x + inner_w - vp_w, inner_y, vidpid, accent);

        inner_y += 16;

        /* Row 2: Vendor + Device name */
        const char* vname = pci_vendor_to_string(dev->vendor_id);
        const char* dname = pci_device_to_string(dev->vendor_id, dev->device_id);
        char vendor_dev[128];
        snprintf(vendor_dev, sizeof(vendor_dev), "%s %s", vname, dname);

        int max_chars = (inner_w) / 8;
        if ((int)gfx_strlen(vendor_dev) > max_chars && max_chars > 3) {
            vendor_dev[max_chars - 3] = '.'; vendor_dev[max_chars - 2] = '.'; vendor_dev[max_chars - 1] = '.'; vendor_dev[max_chars] = 0;
        }
        gfx_draw_string_transparent(inner_x, inner_y, vendor_dev, text);
        inner_y += 14;

        /* Row 3: Class/Subclass/ProgIF + IRQ + BAR summary */
        const char* cls = pci_get_class_name(dev->class_id);
        char detail[128];
        if (dev->interrupt_line != 0xFF && dev->interrupt_line != 0) {
            snprintf(detail, sizeof(detail), "%s / %02X / IRQ %u", cls, dev->subclass, (unsigned)dev->interrupt_line);
        } else {
            snprintf(detail, sizeof(detail), "%s / %02X", cls, dev->subclass);
        }
        gfx_draw_string_transparent(inner_x, inner_y, detail, dim);

        /* BAR summary */
        char bar_sum[64] = "";
        int bar_off = 0;
        for (int b = 0; b < 6 && bar_off < (int)(sizeof(bar_sum) - 16); b++) {
            if (dev->bar[b] & 1) {
                snprintf(bar_sum + bar_off, sizeof(bar_sum) - bar_off, "I/O%X=%X ", b, dev->bar[b] & ~1);
            } else if (dev->bar[b]) {
                snprintf(bar_sum + bar_off, sizeof(bar_sum) - bar_off, "MEM%X=%X ", b, dev->bar[b] >> 4);
            }
            bar_off = (int)gfx_strlen(bar_sum);
        }
        int bar_len = (int)gfx_strlen(bar_sum);
        if (bar_len > 0) {
            int bw = bar_len * 8;
            gfx_draw_string_transparent(inner_x + inner_w - bw, inner_y, bar_sum, 0x6E7681);
        }

        cur_y += item_h;
    }

    /* Footer */
    int fy = y + h - 20;
    gfx_fill_rect(x + 12, fy - 4, w - 24, 1, border);
    char pg[16];
    snprintf(pg, sizeof(pg), "%d / %d", total, total);
    int pw = gfx_strlen(pg) * 8;
    gfx_draw_string_transparent(x + (w - pw) / 2, fy + 4, pg, dim);
}

static void pci_on_resize(int win_id, int w, int h) {
    wm_window_t* win = wm_get_window(win_id);
    if (win) win->content_h = 900;
    last_pci_win = win;
}

void pci_scanner_app(void) {
    pci_scroll = 0;
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 800, win_h = 520;
    int win = wm_open_window((fw - win_w) / 2, (fh - win_h) / 2, win_w, win_h, "PCI Professional Inventory",
                             get_accent_color(), pci_scanner_render, NULL, pci_on_resize);
    if (win >= 0) {
        wm_window_t* wptr = wm_get_window(win);
        if (wptr) wptr->content_h = 900;
        last_pci_win = wptr;
    }
}

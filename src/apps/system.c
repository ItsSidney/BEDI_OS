// ============================================================
//  BEDI OS — Professional System Utility (Restored Logic)
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/bus/pci.h"
#include <stddef.h>

static int system_win_id = -1;

static void system_render(int id, int x, int y, int w, int h, int vx, int vy) {
    personalization_t* p = get_personalization();
    uint32_t text_clr = (p->theme == 0) ? 0xFFFFFF : 0x000000;
    uint32_t bg = (p->theme == 0) ? 0x1A1A1A : 0xF5F5F5;
    uint32_t accent = get_accent_color();
    uint32_t dim_text = (p->theme == 0) ? 0x888888 : 0x555555;

    gfx_fill_rect(x, y, w, h, bg);
    
    // Header (Fixed)
    gfx_draw_string_transparent(x + 20, y + 20 - vy, "SYSTEM HARDWARE INVENTORY", accent);
    gfx_draw_hline(x + 20, y + 42 - vy, w - 40, (p->theme == 0 ? 0x333333 : 0xAAAAAA));

    int total_devices = pci_get_device_count();
    int current_y = y + 60 - vy;
    
    // Restoration of the "Old" PCI Scanner logic: Device list with full info
    for (int i = 0; i < total_devices; i++) {
        pci_device_t* dev = pci_get_device(i);
        if (!dev) continue;

        // Draw Row Bevel if hovered (concept)
        if (i % 2 == 0) {
            gfx_fill_rect_alpha(x + 10, current_y - 4, w - 20, 24, (p->theme == 0 ? 0xFFFFFF : 0x000000), 10);
        }

        // 1. Bus:Slot.Func
        char bsf[16] = "[00:00.0]";
        bsf[1] = (dev->bus / 10) + '0'; bsf[2] = (dev->bus % 10) + '0';
        bsf[4] = (dev->slot / 10) + '0'; bsf[5] = (dev->slot % 10) + '0';
        bsf[7] = (dev->func % 10) + '0';
        gfx_draw_string_transparent(x + 20, current_y, bsf, dim_text);

        // 2. Vendor:Device ID (Hex-like)
        const char hex[] = "0123456789ABCDEF";
        char vid[12] = "0000:0000";
        vid[0] = hex[(dev->vendor_id >> 12) & 0xF]; vid[1] = hex[(dev->vendor_id >> 8) & 0xF];
        vid[2] = hex[(dev->vendor_id >> 4) & 0xF]; vid[3] = hex[dev->vendor_id & 0xF];
        vid[5] = hex[(dev->device_id >> 12) & 0xF]; vid[6] = hex[(dev->device_id >> 8) & 0xF];
        vid[7] = hex[(dev->device_id >> 4) & 0xF]; vid[8] = hex[dev->device_id & 0xF];
        gfx_draw_string_transparent(x + 100, current_y, vid, accent);

        // 3. Class Name
        const char* cls = pci_get_class_name(dev->class_id);
        gfx_draw_string_transparent(x + 200, current_y, cls, text_clr);

        current_y += 28;
    }
}

static void system_on_resize(int win_id, int w, int h) {
    wm_window_t* win = wm_get_window(win_id);
    if (win) {
        int dev_count = pci_get_device_count();
        win->content_h = 80 + dev_count * 28;
    }
}

void system_app(void) {
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 640, win_h = 500;
    system_win_id = wm_open_window((fw - win_w)/2, (fh - win_h)/2, win_w, win_h, "System Tools", get_accent_color(), system_render, 0, system_on_resize);
    system_on_resize(system_win_id, win_w, win_h);
}

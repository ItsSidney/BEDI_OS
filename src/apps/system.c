// ============================================================
//  BEDI OS — Modern System Information Center
// ============================================================
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "gui/wm.h"
#include "gui/gui.h"
#include "drivers/bus/pci.h"
#include "drivers/storage/storage.h"
#include "kernel/net/if.h"
#include "kernel/mem/kheap.h"
#include "kernel/task/task.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>

extern uint64_t get_total_memory_bytes(void);
extern int get_task_count(void);

static int system_win_id = -1;

static uint32_t cpu_get_vendor(char* out) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    out[0] = (ebx >> 0) & 0xFF; out[1] = (ebx >> 8) & 0xFF; out[2] = (ebx >> 16) & 0xFF; out[3] = (ebx >> 24) & 0xFF;
    out[4] = (edx >> 0) & 0xFF; out[5] = (edx >> 8) & 0xFF; out[6] = (edx >> 16) & 0xFF; out[7] = (edx >> 24) & 0xFF;
    out[8] = (ecx >> 0) & 0xFF; out[9] = (ecx >> 8) & 0xFF; out[10] = (ecx >> 16) & 0xFF; out[11] = (ecx >> 24) & 0xFF;
    out[12] = 0;
    return eax;
}

static int cpu_get_brand(char* out) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
    if (eax < 0x80000004) return 0;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002));
    *((uint32_t*)(out + 0)) = eax; *((uint32_t*)(out + 4)) = ebx; *((uint32_t*)(out + 8)) = ecx; *((uint32_t*)(out + 12)) = edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000003));
    *((uint32_t*)(out + 16)) = eax; *((uint32_t*)(out + 20)) = ebx; *((uint32_t*)(out + 24)) = ecx; *((uint32_t*)(out + 28)) = edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000004));
    *((uint32_t*)(out + 32)) = eax; *((uint32_t*)(out + 36)) = ebx; *((uint32_t*)(out + 40)) = ecx; *((uint32_t*)(out + 44)) = edx;
    out[48] = 0;
    int len = 48; while (len > 0 && out[len - 1] == ' ') len--;
    out[len] = 0;
    return 1;
}

static void draw_section_header(int x, int y, const char* title, uint32_t accent) {
    gfx_fill_rect(x + 12, y + 6, 4, 16, accent);
    gfx_draw_string_transparent(x + 22, y + 8, title, 0xFFFFFF);
    gfx_fill_rect(x + 12, y + 28, 320, 1, 0x333333);
}

static void draw_kv(int x, int y, const char* key, const char* val, uint32_t val_clr) {
    gfx_draw_string_transparent(x + 12, y, key, 0x9CA3AF);
    gfx_draw_string_transparent(x + 140, y, val, val_clr);
}

static void system_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx;
    uint32_t bg = 0x0D1117;
    uint32_t card_bg = 0x161B22;
    uint32_t text = 0xE6EDF3;
    uint32_t accent = get_accent_color();
    uint32_t dim = 0x8B949E;
    uint32_t border = 0x30363D;

    gfx_fill_rect(x, y, w, h, bg);

    int cur_y = y + 8 - vy;
    int card_x = x + 12;
    int card_w = w - 24;

    /* Title */
    gfx_draw_string_transparent(card_x, cur_y, "SYSTEM INFORMATION", accent);
    cur_y += 22;
    gfx_fill_rect(card_x, cur_y, card_w, 1, border);
    cur_y += 10;

    /* CPU */
    draw_section_header(card_x, cur_y, "Central Processing Unit", 0x58A6FF);
    cur_y += 36;

    char vendor[13] = {0};
    cpu_get_vendor(vendor);

    char brand[49] = {0};
    int has_brand = cpu_get_brand(brand);

    char buf[64];
    draw_kv(card_x, cur_y, "Vendor", vendor, dim); cur_y += 18;
    if (has_brand) {
        draw_kv(card_x, cur_y, "Brand", brand, text); cur_y += 18;
    }

    draw_kv(card_x, cur_y, "Architecture", "x86_64", text); cur_y += 18;
    snprintf(buf, sizeof(buf), "%d-bit", 64); draw_kv(card_x, cur_y, "Word Size", buf, text); cur_y += 18;
    cur_y += 6;

    /* Memory */
    draw_section_header(card_x, cur_y, "Memory", 0x3FB950);
    cur_y += 36;

    uint64_t total_mem = get_total_memory_bytes();
    size_t heap_free = kheap_free();
    snprintf(buf, sizeof(buf), "%u MB", (unsigned)(total_mem >> 20));
    draw_kv(card_x, cur_y, "Total RAM", buf, text); cur_y += 18;
    snprintf(buf, sizeof(buf), "%u MB", (unsigned)(heap_free >> 20));
    draw_kv(card_x, cur_y, "Heap Free", buf, text); cur_y += 18;
    snprintf(buf, sizeof(buf), "%u KB", (unsigned)(heap_free >> 10));
    draw_kv(card_x, cur_y, "Available", buf, dim); cur_y += 18;
    cur_y += 6;

    /* Display */
    draw_section_header(card_x, cur_y, "Display", 0xF0883E);
    cur_y += 36;

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    snprintf(buf, sizeof(buf), "%u x %u", (unsigned)fw, (unsigned)fh);
    draw_kv(card_x, cur_y, "Resolution", buf, text); cur_y += 18;
    draw_kv(card_x, cur_y, "Depth", "32-bit", text); cur_y += 18;
    cur_y += 6;

    /* Storage */
    draw_section_header(card_x, cur_y, "Storage", 0xA371F7);
    cur_y += 36;

    uint32_t dev_count = storage_get_device_count();
    if (dev_count > 0) {
        for (uint32_t si = 0; si < dev_count && si < 8; si++) {
            block_device_t* dev = storage_get_device(si);
            if (!dev) continue;
            uint64_t mb = (dev->size_sectors * dev->block_size) >> 20;
            snprintf(buf, sizeof(buf), "%s — %u MB (%u sectors)", dev->name, (unsigned)mb, (unsigned)dev->size_sectors);
            draw_kv(card_x, cur_y, "Device", buf, text); cur_y += 18;
        }
    } else {
        draw_kv(card_x, cur_y, "Storage", "No devices detected", dim); cur_y += 18;
    }
    cur_y += 6;

    /* Network */
    draw_section_header(card_x, cur_y, "Network Interfaces", 0x39D399);
    cur_y += 36;

    struct ifnet* ifp = if_list_head();
    int net_count = 0;
    while (ifp) {
        snprintf(buf, sizeof(buf), "%s", ifp->if_xname);
        draw_kv(card_x, cur_y, "Interface", buf, text); cur_y += 18;
        snprintf(buf, sizeof(buf), "%s", (ifp->if_flags & IFF_UP) ? "Up" : "Down");
        draw_kv(card_x, cur_y, "Status", buf, (ifp->if_flags & IFF_UP) ? text : 0xF0883E); cur_y += 18;
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u",
            (ifp->if_ip >> 24) & 0xFF, (ifp->if_ip >> 16) & 0xFF,
            (ifp->if_ip >> 8) & 0xFF, ifp->if_ip & 0xFF);
        draw_kv(card_x, cur_y, "IP Address", buf, text); cur_y += 18;
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
            ifp->if_hwaddr[0], ifp->if_hwaddr[1], ifp->if_hwaddr[2],
            ifp->if_hwaddr[3], ifp->if_hwaddr[4], ifp->if_hwaddr[5]);
        draw_kv(card_x, cur_y, "MAC", buf, text); cur_y += 18;
        snprintf(buf, sizeof(buf), "%u", (unsigned)ifp->if_mtu);
        draw_kv(card_x, cur_y, "MTU", buf, dim); cur_y += 18;
        cur_y += 4;
        net_count++;
        ifp = ifp->if_next;
    }
    if (net_count == 0) {
        draw_kv(card_x, cur_y, "Network", "No interfaces found", dim); cur_y += 18;
    }
    cur_y += 6;

    /* PCI */
    draw_section_header(card_x, cur_y, "PCI Devices", 0xF2CC8C);
    cur_y += 36;

    int pci_count = pci_get_device_count();
    snprintf(buf, sizeof(buf), "%d device(s) enumerated", pci_count);
    draw_kv(card_x, cur_y, "Devices", buf, text); cur_y += 18;
    draw_kv(card_x, cur_y, "Max", "64 (internal pool)", dim); cur_y += 18;
    cur_y += 6;

    /* Tasks */
    draw_section_header(card_x, cur_y, "Kernel / Tasks", 0xE893B0);
    cur_y += 36;

    draw_kv(card_x, cur_y, "Type", "Monolithic Kernel", text); cur_y += 18;
    draw_kv(card_x, cur_y, "Arch", "x86_64", text); cur_y += 18;
    snprintf(buf, sizeof(buf), "%d", get_task_count());
    draw_kv(card_x, cur_y, "Running Tasks", buf, text); cur_y += 18;
    draw_kv(card_x, cur_y, "Scheduler", "Preemptive", dim); cur_y += 18;
    cur_y += 6;
}

static void system_on_resize(int win_id, int w, int h) {
    wm_window_t* win = wm_get_window(win_id);
    if (win) {
        win->content_h = 900;
    }
}

void system_app(void) {
    if (system_win_id >= 0) {
        wm_close_window(system_win_id);
        system_win_id = -1;
        return;
    }
    uint32_t fw = get_fb_width(), fh = get_fb_height();
    int win_w = 720, win_h = 520;
    int x = (fw - win_w) / 2;
    int y = (fh - win_h) / 2;
    system_win_id = wm_open_window(x, y, win_w, win_h, "System Information", get_accent_color(),
                         system_render, NULL, system_on_resize);
    if (system_win_id >= 0) {
        wm_window_t* win = wm_get_window(system_win_id);
        if (win) win->content_h = 900;
    }
}

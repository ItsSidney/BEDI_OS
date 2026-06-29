#include "gui/gui.h"
#include "gui/wm.h"
#include "drivers/video/gfx.h"
#include "drivers/input/keyboard.h"
#include "kernel/task/task.h"
#include "kernel/time/timer.h"
#include "kernel/mem/kheap.h"
#include "kernel/mem/vmm.h"
#include "kernel/acpi.h"
#include "drivers/video/framebuffer.h"
#include "drivers/video/gpu.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

extern uint64_t get_total_memory_bytes(void);

#define PF_SAMPLES 60
#define PF_REFRESH_MS 500

typedef struct {
    int cpu_samples[PF_SAMPLES];
    int cpu_idx;
    int cpu_count;

    int ram_samples[PF_SAMPLES];
    int ram_idx;
    int ram_count;

    uint32_t last_tick;
    uint32_t prev_render;

    uint64_t prev_sched;
    uint64_t prev_busy;

    char cpu_brand[64];
    char gpu_vendor[32];
    char gpu_res[32];
    char gpu_vram[32];
    uint32_t total_mem;
    uint32_t page_size;
    int gpu_caps;
    int gpu_present;
} perf_state_t;

static int pf_win_id = -1;

static void pf_detect_cpu(perf_state_t* ps) {
    uint32_t eax, ebx, ecx, edx;
    __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
    if (eax >= 0x80000004) {
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002));
        *((uint32_t*)(ps->cpu_brand + 0)) = eax;
        *((uint32_t*)(ps->cpu_brand + 4)) = ebx;
        *((uint32_t*)(ps->cpu_brand + 8)) = ecx;
        *((uint32_t*)(ps->cpu_brand + 12)) = edx;
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000003));
        *((uint32_t*)(ps->cpu_brand + 16)) = eax;
        *((uint32_t*)(ps->cpu_brand + 20)) = ebx;
        *((uint32_t*)(ps->cpu_brand + 24)) = ecx;
        *((uint32_t*)(ps->cpu_brand + 28)) = edx;
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000004));
        *((uint32_t*)(ps->cpu_brand + 32)) = eax;
        *((uint32_t*)(ps->cpu_brand + 36)) = ebx;
        *((uint32_t*)(ps->cpu_brand + 40)) = ecx;
        *((uint32_t*)(ps->cpu_brand + 44)) = edx;
        ps->cpu_brand[48] = 0;
        int len = 48;
        while (len > 0 && ps->cpu_brand[len-1] == ' ') len--;
        ps->cpu_brand[len] = 0;
    } else {
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
        *((uint32_t*)(ps->cpu_brand + 0)) = ebx;
        *((uint32_t*)(ps->cpu_brand + 4)) = edx;
        *((uint32_t*)(ps->cpu_brand + 8)) = ecx;
        ps->cpu_brand[12] = 0;
    }
}

static void pf_sample(perf_state_t* ps) {
    uint32_t now = timer_get_ms();
    if (now - ps->last_tick < PF_REFRESH_MS) return;
    ps->last_tick = now;

    uint64_t sched = cpu_sched_ticks;
    uint64_t busy = cpu_busy_ticks;
    uint64_t ds = sched - ps->prev_sched;
    uint64_t db = busy - ps->prev_busy;
    int cpu_pct = (ds > 0) ? (int)((db * 100) / ds) : 0;
    if (cpu_pct > 100) cpu_pct = 100;
    if (cpu_pct < 0) cpu_pct = 0;
    ps->prev_sched = sched;
    ps->prev_busy = busy;

    ps->cpu_samples[ps->cpu_idx] = cpu_pct;
    ps->cpu_idx = (ps->cpu_idx + 1) % PF_SAMPLES;
    if (ps->cpu_count < PF_SAMPLES) ps->cpu_count++;

    size_t free_heap = kheap_free();
    uint64_t used_heap = KHEAP_SIZE - free_heap;
    int ram_pct = (int)((used_heap * 100) / KHEAP_SIZE);
    if (ram_pct > 100) ram_pct = 100;

    ps->ram_samples[ps->ram_idx] = ram_pct;
    ps->ram_idx = (ps->ram_idx + 1) % PF_SAMPLES;
    if (ps->ram_count < PF_SAMPLES) ps->ram_count++;
}

static void pf_draw_graph(int gx, int gy, int gw, int gh, int* samples, int count, int idx, uint32_t line_color, uint32_t fill_color) {
    if (count < 2) return;

    int usable_w = gw - 2;
    int usable_h = gh - 2;

    int n = (count < PF_SAMPLES) ? count : PF_SAMPLES;
    int start = (count < PF_SAMPLES) ? 0 : idx;

    int prev_x = -1, prev_y = -1;
    for (int i = 0; i < n; i++) {
        int si = (start + i) % PF_SAMPLES;
        int val = samples[si];
        if (val < 0) val = 0;
        if (val > 100) val = 100;

        int px = gx + 1 + (i * usable_w) / (n - 1);
        int py = gy + 1 + usable_h - (val * usable_h) / 100;

        if (i > 0 && prev_x >= 0) {
            gfx_draw_line(prev_x, prev_y, px, py, line_color);
        }

        prev_x = px;
        prev_y = py;
    }

    int last_val = 0;
    if (count > 0) {
        int li = (idx - 1 + PF_SAMPLES) % PF_SAMPLES;
        last_val = samples[li];
        if (last_val < 0) last_val = 0;
        if (last_val > 100) last_val = 100;
    }

    if (prev_x > gx + 4 && prev_y > gy + 4 && prev_y < gy + gh - 4) {
        gfx_fill_circle(prev_x, prev_y, 3, line_color);
        gfx_fill_circle(prev_x, prev_y, 2, fill_color);
    }

    char pct_buf[8];
    itoa(last_val, pct_buf);
    int sl = 0;
    while (pct_buf[sl]) sl++;
    pct_buf[sl++] = '%'; pct_buf[sl] = 0;
    gfx_draw_string_transparent(gx + gw - 44, gy + 4, pct_buf, line_color);
}

static void pf_render(int id, int x, int y, int w, int h, int vx, int vy) {
    (void)id; (void)vx; (void)vy;
    perf_state_t* ps = (perf_state_t*)wm_get_app_data(id);
    if (!ps) return;

    pf_sample(ps);

    uint32_t bg = 0x0D1117;
    uint32_t card = 0x161B22;
    uint32_t border = 0x30363D;
    uint32_t text = 0xC9D1D9;
    uint32_t dim = 0x6E7681;

    gfx_fill_rect(x, y, w, h, bg);

    int cx = x + 10;
    int cw = w - 20;
    int graph_h = (h - 56) / 3;
    int gap = 4;

    char buf[64];

    int panel_y = y + 8;

    for (int section = 0; section < 3; section++) {
        int py = panel_y + section * (graph_h + gap);

        gfx_fill_rect(cx, py, cw, graph_h, card);
        gfx_draw_rect_outline(cx, py, cw, graph_h, 1, border);

        int label_y = py + 6;
        int graph_x = cx + 10;
        int graph_y = py + 24;
        int graph_w = cw - 20;
        int graph_h_inner = graph_h - 28;

        if (section == 0) {
            uint32_t cpu_color = 0x58A6FF;
            gfx_draw_string_transparent(cx + 8, label_y, "CPU", cpu_color);
            gfx_draw_string_transparent(cx + 46, label_y, ps->cpu_brand, dim);
            pf_draw_graph(graph_x, graph_y, graph_w, graph_h_inner,
                ps->cpu_samples, ps->cpu_count, ps->cpu_idx,
                cpu_color, 0x0D1117);

            int cpu_val = 0;
            if (ps->cpu_count > 0) {
                int li = (ps->cpu_idx - 1 + PF_SAMPLES) % PF_SAMPLES;
                cpu_val = ps->cpu_samples[li];
                if (cpu_val < 0) cpu_val = 0;
            }
            itoa(cpu_val, buf);
            int bl = 0;
            while (buf[bl]) bl++;
            buf[bl++] = '%'; buf[bl] = 0;
            gfx_draw_string_transparent(cx + cw - 52, label_y, buf, cpu_color);

        } else if (section == 1) {
            uint32_t ram_color = 0x3FB950;
            gfx_draw_string_transparent(cx + 8, label_y, "RAM", ram_color);

            size_t fh = kheap_free();
            uint32_t uh = (KHEAP_SIZE - fh) >> 10;
            uint32_t th = KHEAP_SIZE >> 10;
            snprintf(buf, 64, "%u KB / %u KB  (phys %u MB)", uh, th, ps->total_mem >> 20);
            gfx_draw_string_transparent(cx + 46, label_y, buf, dim);

            pf_draw_graph(graph_x, graph_y, graph_w, graph_h_inner,
                ps->ram_samples, ps->ram_count, ps->ram_idx,
                ram_color, 0x0D1117);

            int ram_val = 0;
            if (ps->ram_count > 0) {
                int li = (ps->ram_idx - 1 + PF_SAMPLES) % PF_SAMPLES;
                ram_val = ps->ram_samples[li];
                if (ram_val < 0) ram_val = 0;
            }
            itoa(ram_val, buf);
            int bl = 0;
            while (buf[bl]) bl++;
            buf[bl++] = '%'; buf[bl] = 0;
            gfx_draw_string_transparent(cx + cw - 52, label_y, buf, ram_color);

        } else if (section == 2) {
            uint32_t gpu_color = 0xBC8CFF;
            gfx_draw_string_transparent(cx + 8, label_y, "GPU", gpu_color);

            if (ps->gpu_present) {
                gfx_draw_string_transparent(cx + 46, label_y, ps->gpu_vendor, dim);
                gfx_draw_string_transparent(graph_x, graph_y + 4, ps->gpu_res, text);
                gfx_draw_string_transparent(graph_x, graph_y + 24, ps->gpu_vram, dim);

                if (ps->gpu_caps & GPU_CAP_3D) {
                    gfx_draw_string_transparent(graph_x, graph_y + 44, "3D Accelerated", 0x3FB950);
                } else if (ps->gpu_caps & GPU_CAP_2D) {
                    gfx_draw_string_transparent(graph_x, graph_y + 44, "2D Acceleration", 0x58A6FF);
                } else {
                    gfx_draw_string_transparent(graph_x, graph_y + 44, "No acceleration", 0x6E7681);
                }
            } else {
                gfx_draw_string_transparent(graph_x, graph_y + graph_h_inner / 2 - 8, "No GPU detected", dim);
            }
        }
    }

    gfx_draw_hline(x + 8, y + h - 22, w - 16, border);
    gfx_draw_string_transparent(x + 12, y + h - 18, "Q / Esc: Close  |  Refreshes every 500ms", 0x484F58);
}

static void pf_key(int id, char key) {
    (void)id;
    if (key == 'q' || key == 'Q' || key == 27) {
        perf_state_t* ps = (perf_state_t*)wm_get_app_data(id);
        if (ps) kfree(ps);
        wm_set_app_data(id, 0);
        wm_close_window(pf_win_id);
        pf_win_id = -1;
    }
}

static void pf_resize(int id, int w, int h) {
    (void)id; (void)w; (void)h;
}

void perfmon_app(void) {
    if (wm_get_window(pf_win_id)) { wm_bring_to_front(pf_win_id); return; }

    perf_state_t* ps = (perf_state_t*)kmalloc(sizeof(perf_state_t));
    if (!ps) return;
    memset(ps, 0, sizeof(perf_state_t));

    pf_detect_cpu(ps);
    ps->total_mem = (uint32_t)get_total_memory_bytes();
    ps->last_tick = timer_get_ms();
    ps->prev_sched = cpu_sched_ticks;
    ps->prev_busy = cpu_busy_ticks;

    gpu_device_t* gpu = gpu_get_primary();
    if (gpu) {
        ps->gpu_present = 1;
        const char* ven = "Unknown";
        switch (gpu->vendor_id) {
            case 0x8086: ven = "Intel"; break;
            case 0x10DE: ven = "NVIDIA"; break;
            case 0x1002: ven = "AMD"; break;
            case 0x1AF4: ven = "VirtIO"; break;
        }
        snprintf(ps->gpu_vendor, 32, "%s (%04X)", ven, gpu->vendor_id);
        snprintf(ps->gpu_res, 32, "%ux%u", gpu->width, gpu->height);
        if (gpu->vram_size > 0) {
            uint32_t vram_mb = gpu->vram_size >> 20;
            if (vram_mb > 0) snprintf(ps->gpu_vram, 32, "VRAM: %u MB", vram_mb);
            else snprintf(ps->gpu_vram, 32, "VRAM: %u KB", gpu->vram_size >> 10);
        } else {
            snprintf(ps->gpu_vram, 32, "VRAM: N/A");
        }
    } else {
        ps->gpu_present = 0;
    }
    ps->gpu_caps = gpu_get_capabilities();

    uint32_t fw = get_fb_width(), fh = get_fb_height();
    pf_win_id = wm_open_window(
        (fw - 480) / 2,
        (fh - 400) / 2,
        480, 400,
        "Performance",
        get_accent_color(),
        pf_render, pf_key, pf_resize
    );
    wm_set_app_data(pf_win_id, ps);
}

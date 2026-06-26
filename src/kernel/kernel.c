// kernel.c: BEDI OS Kernel entry point
#include "../../include/limine.h"
#include "commands/commands.h"
#include "drivers/video/framebuffer.h"
#include "drivers/video/gpu.h"
#include "drivers/input/keyboard.h"
#include "gui/gui.h"
#include "kernel/arch/x86_64/idt.h"
#include "kernel/time/timer.h"
#include "drivers/video/gfx.h"
#include "drivers/input/mouse.h"
#include "drivers/audio/audio.h"
#include "drivers/bus/pci.h"
#include "kernel/mem/kheap.h"
#include "kernel/mem/vmm.h"
#include "filesystem/filesystem.h"
#include "filesystem/vfs.h"
#include "filesystem/ramfs.h"
#include "filesystem/fat32.h"
#include "drivers/storage/storage.h"
#include "drivers/storage/ide.h"
#include "kernel/task/task.h"
#include "kernel/security/security.h"
#include "kernel/log.h"
#include "kernel/rust_bridge.h"
#include "kernel/arch/x86_64/gdt.h"
#include "kernel/acpi.h"

// ============ SERIAL DEBUG ============
static inline void outb_k(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb_k(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
void serial_init() {
    outb_k(0x3F8+1,0); outb_k(0x3F8+3,0x80); outb_k(0x3F8,3);
    outb_k(0x3F8+1,0); outb_k(0x3F8+3,3); outb_k(0x3F8+2,0xC7); outb_k(0x3F8+4,0x0B);
}
void serial_puts(const char *s) {
    klog(s);
    while (*s) {
        int timeout = 100000;
        while ((inb_k(0x3F8+5) & 0x20) == 0 && timeout-- > 0);
        if (timeout <= 0) break;
        if (*s == '\n') outb_k(0x3F8, '\r');
        outb_k(0x3F8, *s++);
    }
}

// ============ LIMINE REQUESTS ============
__attribute__((used, section(".requests_start_marker")))
static volatile uint64_t limine_requests_start_marker_arr[4] = {
    0xf6b8f4b39de7d1ae, 0xfab91a6940fcb9cf,
    0x785c6ed015d3e316, 0x181e920a7852b9d9
};

__attribute__((used, section(".requests")))
static volatile uint64_t limine_base_revision[3] = {
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 0
};

// Global asset pointers for GUI/App access (Wallpapers, Audio, etc.)
void* asset_data_arr[10] = {0};
uint64_t asset_size_arr[10] = {0};

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

void draw_early_progress(int step, uint32_t color) {
    (void)color;
    const char* tag = 0;
    const char* msg = 0;
    uint32_t tag_color = 0x58A6FF;
    switch (step) {
        case 1:  tag = "BEDI"; msg = "_start_c starting"; tag_color = 0x58A6FF; break;
        case 2:  tag = "BEDI"; msg = "IDT initialized"; tag_color = 0x58A6FF; break;
        case 3:  tag = "BEDI"; msg = "FPU initialized"; tag_color = 0x58A6FF; break;
        case 4:  tag = "BEDI"; msg = "boot modules loaded"; tag_color = 0x58A6FF; break;
        case 5:  tag = "BEDI"; msg = "framebuffer ready"; tag_color = 0x58A6FF; break;
        case 6:  tag = "BEDI"; msg = "HHDM configured"; tag_color = 0x58A6FF; break;
        case 7:  tag = "BEDI"; msg = "memory map read"; tag_color = 0x58A6FF; break;
        case 8:  tag = "BEDI"; msg = "kmain starting..."; tag_color = 0x3FB950; break;
        case 9:  tag = "HEAP"; msg = "kernel heap initialized"; tag_color = 0xF0883E; break;
        case 10: tag = "VMM"; msg = "Virtual Memory Manager initialized"; tag_color = 0x3FB950; break;
        case 11: tag = "TIMER"; msg = "PIT timer set to 1000 Hz"; tag_color = 0xF0883E; break;
        case 12: tag = "TASK"; msg = "multitasking enabled"; tag_color = 0xBC8CFF; break;
        case 14: tag = "SEC"; msg = "security monitor initialized"; tag_color = 0xF85149; break;
        case 15: tag = "RUST"; msg = "Rust kernel module loaded [v0.2.0]"; tag_color = 0xBC8CFF; break;
        case 16: tag = "AUDIO"; msg = "audio subsystem initialized"; tag_color = 0x39D2C0; break;
        case 17: tag = "FS"; msg = "filesystem abstraction ready"; tag_color = 0xE3B341; break;
        case 18: tag = "PCI"; msg = "PCI bus enumerated"; tag_color = 0x58A6FF; break;
        case 19: tag = "ACPI"; msg = "ACPI tables initialized"; tag_color = 0x39D2C0; break;
        case 20: tag = "GPU"; msg = "GPU initialization complete"; tag_color = 0x3FB950; break;
        case 21: tag = "NET"; msg = "networking stack initialized"; tag_color = 0x39D2C0; break;
        case 22: tag = "MOUSE"; msg = "PS/2 mouse driver ready"; tag_color = 0xE3B341; break;
        case 23: tag = "VFS"; msg = "Virtual Filesystem initialized"; tag_color = 0xF0883E; break;
        case 24: tag = "IDE"; msg = "IDE storage initialized"; tag_color = 0x58A6FF; break;
        case 25: tag = "VFS"; msg = "root filesystem mounted"; tag_color = 0x3FB950; break;
        case 31: tag = "BEDI"; msg = "starting GUI..."; tag_color = 0x3FB950; break;
        default:
            if (step >= 26 && step <= 30) { tag = "STORAGE"; msg = "scanning storage device..."; tag_color = 0xF0883E; }
            else if (step >= 32) { tag = "PCI"; msg = "probing PCI bus..."; tag_color = 0x58A6FF; }
            else { tag = "BEDI"; msg = "initializing..."; tag_color = 0x8B949E; }
            break;
    }
    if (tag && msg) {
        extern void boot_log_add(const char*, const char*, uint32_t, uint32_t);
        extern void draw_splash_screen(int);
        boot_log_add(tag, msg, tag_color, 0xE8EAED);
        draw_splash_screen(step);
    }
}

__attribute__((used, section(".requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

uint64_t hhdm_offset = 0;

uint64_t get_total_memory_bytes(void) {
    if (memmap_request.response == 0) return 0;
    uint64_t total = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count; i++) {
        total += memmap_request.response->entries[i]->length;
    }
    return total;
}

// ============ ASSET EXPORT ============
void* get_asset_data(int idx) { 
    if (idx < 0 || idx >= 10) return 0;
    return asset_data_arr[idx]; 
}
uint64_t get_asset_size(int idx) { 
    if (idx < 0 || idx >= 10) return 0;
    return asset_size_arr[idx]; 
}



__attribute__((used, section(".requests_end_marker")))
static volatile uint64_t limine_requests_end_marker_arr[2] = {
    0xadc0e0531bb10d03, 0x9572709f31764c62
};

// ============ KERNEL CODE ============
static void hcf(void) {
    __asm__ ("cli");
    for (;;) { __asm__ ("hlt"); }
}

void delay(long count) {
    for (volatile long i = 0; i < count; i++);
}

extern void play_beep();
void kmain(void);

volatile int background_counter = 0;
extern void net_poll();
void background_worker(void) {
    while (1) {
        background_counter++;
        net_poll();
        sleep_task(50); // Sleep 50ms
    }
}

#include "kernel/net/socket.h"
extern void itoa(uint64_t n, char* s);
extern void exit_task(int code);
#include <string.h>

extern void init_fpu();

void _start_c(void) {
    serial_init();
//    serial_puts("[BEDI] _start_c starting\n");

    if (framebuffer_request.response != 0 && framebuffer_request.response->framebuffer_count >= 1) {
//        serial_puts("[BEDI] Initializing framebuffer\n");
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        init_framebuffer(
            (uint32_t*)fb->address,
            fb->width,
            fb->height,
            fb->pitch,
            fb->bpp,
            fb->red_mask_shift,
            fb->green_mask_shift,
            fb->blue_mask_shift
        );
//        serial_puts("[BEDI] Framebuffer initialized\n");
    }

//    serial_puts("[BEDI] Calling draw_early_progress\n");
    draw_early_progress(1, 0x00FF00); // 1: Serial
//    serial_puts("[BEDI] draw_early_progress returned\n");

    init_idt();
    draw_early_progress(2, 0x00FF00); // 2: IDT

    init_fpu();
    draw_early_progress(3, 0x00FF00); // 3: FPU

    if (limine_base_revision[2] != 0) {
//        serial_puts("[BEDI] FATAL: base revision\n");
        draw_early_progress(1, 0xFF0000); 
        hcf();
    }

    if (module_request.response != 0 && module_request.response->module_count > 0) {
        for (uint64_t i = 0; i < module_request.response->module_count && i < 10; i++) {
            struct limine_file *f = module_request.response->modules[i];
            asset_data_arr[i] = f->address;
            asset_size_arr[i] = f->size;
        }
    }
    draw_early_progress(4, 0x00FF00); // 4: Modules

    draw_early_progress(5, 0x00FF00); // 5: Framebuffer

    if (hhdm_request.response == 0) {
//        serial_puts("[BEDI] FATAL: no HHDM\n");
        draw_early_progress(3, 0xFF0000); 
        hcf();
    }
    hhdm_offset = hhdm_request.response->offset;
    draw_early_progress(6, 0x00FF00); // 6: HHDM

    if (memmap_request.response == 0) {
//        serial_puts("[BEDI] FATAL: no memmap\n");
        draw_early_progress(4, 0xFF0000); 
        hcf();
    }
    draw_early_progress(7, 0x00FF00); // 7: Memmap

    kmain();
    hcf();
}

void kmain() {
//    serial_puts("[BEDI] kmain starting\n");
    draw_early_progress(8, 0x00FF00); // 8: kmain

    kheap_init();
    draw_early_progress(9, 0x00FF00); // 9: Heap
    
    log_init();
    klog("[BEDI] Kernel log started\n");

    vmm_init();
    draw_early_progress(10, 0x00FF00); // 10: VMM
    
    init_timer(1000);
    draw_early_progress(11, 0x00FF00); // 11: Timer
    
    init_tasking();
    draw_early_progress(12, 0x00FF00); // 12: Tasking
    
    // init_syscall_gs_base();
    // klog("Initializing TSS and GS base...\n");
    // init_syscall_gs_base();
    // klog("TSS and GS base done\n");
    // draw_early_progress(13, 0x00FF00); // 13: Syscall/GS
    
    init_security();
    draw_early_progress(14, 0x00FF00); // 14: Security

    rust_kernel_init();
    draw_early_progress(15, 0x00FF00); // 14: Rust

    audio_init();
    draw_early_progress(16, 0x00FF00); // 15: Audio

    init_filesystem();
    draw_early_progress(17, 0x00FF00); // 16: Filesystem

    pci_init();
    draw_early_progress(18, 0x00FF00); // 17: PCI
    
    // ACPI (non-fatal if missing - graceful fallback)
    if (acpi_init() == 0) {
        draw_early_progress(19, 0x00FF00); // 18: ACPI
    } else {
        draw_early_progress(19, 0xF0883E); // ACPI - degraded mode (orange)
    }
    
    gpu_init();
    draw_early_progress(20, 0x00FF00); // 19: GPU

    __asm__ volatile("sti");

    extern void net_init();
    net_init();
    draw_early_progress(21, 0x00FF00); // 20: Network
    
    init_mouse();
    mouse_set_bounds(get_fb_width(), get_fb_height());
    draw_early_progress(22, 0x00FF00); // 21: Mouse
    
    vfs_init();
    draw_early_progress(23, 0x00FF00); // 22: VFS
    
    ide_init();
    draw_early_progress(24, 0x00FF00); // 23: IDE
    
    vfs_mount("/", ramfs_init());
    draw_early_progress(25, 0x00FF00); // 24: Mount
    uint32_t dev_count = storage_get_device_count();
    for (uint32_t i = 0; i < dev_count && i < 6; i++) {
        draw_early_progress(26 + i, 0x00FF00);
        block_device_t* dev = storage_get_device(i);
        vfs_node_t* partition = fat32_init(dev, 0);
        if (partition) {
            vfs_mount(dev->name, partition);
        }
    }
    create_task(background_worker, "Worker");
    
//    serial_puts("[BEDI] Starting GUI\n");
    draw_early_progress(31, 0x00FF00); // GUI
    
    extern void set_splash_mode(bool mode);
    set_splash_mode(false);
    
    start_gui();    // Boot to GUI

    // If GUI exits for some reason, just halt
    while(1) {
        __asm__ volatile("hlt");
    }
}

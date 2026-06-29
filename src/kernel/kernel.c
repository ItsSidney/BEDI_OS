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
    extern void boot_log_add(const char*, const char*, uint32_t, uint32_t);
    extern void draw_boot_log(void);
    char buf[16];
    const char* msgs[] = {
        "Serial", "IDT", "FPU", "Modules", "Framebuffer",
        "HHDM", "Memmap", "kmain", "Heap", "VMM",
        "Timer", "Tasking", "Security", "Rust", "Audio",
        "Filesystem", "PCI", "ACPI", "GPU", "Network",
        "Mouse", "VFS", "IDE", "Mount", "GUI"
    };
    const char* msg = (step > 0 && step <= (int)(sizeof(msgs)/sizeof(msgs[0]))) ? msgs[step - 1] : "Init";
    boot_log_add("INIT", msg, color, 0xFFFFFF);
    static int boot_draw_counter = 0;
    if (++boot_draw_counter >= 3) {
        boot_draw_counter = 0;
        draw_boot_log();
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
extern void net_init();

void _start_c(void) {
    serial_init();

    if (framebuffer_request.response != 0 && framebuffer_request.response->framebuffer_count >= 1) {
        struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];
        uint64_t fb_addr_virt = (uint64_t)fb->address;
        boot_log_add_hex("FB", "Framebuffer base", 0x58A6FF, 0xE8EAED, fb_addr_virt, 0x3FB950);
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
        boot_log_add("FB", "Mode initialized", 0x58A6FF, 0xE8EAED);
        char resbuf[32];
        itoa(fb->width, resbuf); boot_log_add("FB", resbuf, 0x58A6FF, 0xE8EAED);
        itoa(fb->height, resbuf); boot_log_add("FB", resbuf, 0x58A6FF, 0xE8EAED);
    } else {
        boot_log_add("FB", "No framebuffer", 0xFF0000, 0xF85149);
        hcf();
    }
    draw_early_progress(1, 0x00FF00);

    init_idt();
    draw_early_progress(2, 0x00FF00);

    init_fpu();
    draw_early_progress(3, 0x00FF00);

    if (limine_base_revision[2] != 0) {
        draw_early_progress(1, 0xFF0000); 
        hcf();
    }

    if (module_request.response != 0 && module_request.response->module_count > 0) {
        for (uint64_t i = 0; i < module_request.response->module_count && i < 10; i++) {
            struct limine_file *f = module_request.response->modules[i];
            asset_data_arr[i] = f->address;
            asset_size_arr[i] = f->size;
            boot_log_add_hex("MOD", "Module loaded", 0xBC8CFF, 0xE8EAED, (uint64_t)f->address, 0x3FB950);
            char szbuf[20];
            itoa(f->size, szbuf);
            boot_log_add("MOD", szbuf, 0xBC8CFF, 0xE8EAED);
        }
    } else {
        boot_log_add("MOD", "No modules", 0xF0883E, 0xE8EAED);
    }
    draw_early_progress(4, 0x00FF00);
    draw_early_progress(5, 0x00FF00);

    if (hhdm_request.response == 0) {
        draw_early_progress(4, 0xFF0000); 
        hcf();
    }
    hhdm_offset = hhdm_request.response->offset;
    boot_log_add_hex("HHDM", "Higher-half direct map", 0x39D2C0, 0xE8EAED, hhdm_offset, 0x3FB950);
    draw_early_progress(6, 0x00FF00);

    if (memmap_request.response == 0) {
        draw_early_progress(5, 0xFF0000); 
        hcf();
    }
    // Dump memory map entries
    uint64_t total_mem = 0;
    for (uint64_t i = 0; i < memmap_request.response->entry_count && i < 16; i++) {
        struct limine_memmap_entry *e = memmap_request.response->entries[i];
        total_mem += e->length;
        boot_log_add_hex("MEM", "Region", 0x39D2C0, 0xE8EAED, e->base, 0xF0F6FC);
        boot_log_add_hex("MEM", "  length", 0x39D2C0, 0xE8EAED, e->length, 0xF0F6FC);
        char typebuf[16];
        itoa(e->type, typebuf);
        boot_log_add("MEM", "  type", 0x39D2C0, 0xE8EAED);
    }
    char totalbuf[32];
    itoa(total_mem, totalbuf);
    boot_log_add("MEM", "Total usable", 0x39D2C0, 0xE8EAED);
    draw_early_progress(7, 0x00FF00);

    kmain();
    hcf();
}

void kmain() {
    draw_early_progress(8, 0x00FF00); // kmain

    kheap_init();
    boot_log_add_hex("HEAP", "Kernel heap size", 0x39D2C0, 0xE8EAED, (uint64_t)KHEAP_SIZE, 0x3FB950);
    draw_early_progress(9, 0x00FF00); // Heap
    
    log_init();
    klog("[BEDI] Kernel log started\n");

    vmm_init();
    {
        uint64_t cr3_val;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3_val));
        boot_log_add_hex("VMM", "CR3 (PML4 phys)", 0x39D2C0, 0xE8EAED, cr3_val, 0x3FB950);
    }
    draw_early_progress(10, 0x00FF00); // VMM
    
    init_timer(1000);
    boot_log_add_hex("TIMR", "PIT frequency Hz", 0x39D2C0, 0xE8EAED, 1000ULL, 0x3FB950);
    draw_early_progress(11, 0x00FF00); // Timer
    
    init_tasking();
    boot_log_add("TSK", "Task subsystem online", 0x39D2C0, 0xE8EAED);
    draw_early_progress(12, 0x00FF00); // Tasking
    
    init_security();
    boot_log_add("SEC", "users initialized (root, guest)", 0xF85149, 0xE8EAED);
    draw_early_progress(13, 0x00FF00); // Security

    rust_kernel_init();
    draw_early_progress(14, 0x00FF00); // Rust

    audio_init();
    boot_log_add("SND", "Audio pipeline ready", 0xBC8CFF, 0xE8EAED);
    draw_early_progress(15, 0x00FF00); // Audio

    init_filesystem();
    boot_log_add("FS", "Filesystem core ready", 0x39D2C0, 0xE8EAED);
    draw_early_progress(16, 0x00FF00); // Filesystem

    pci_init();
    draw_early_progress(17, 0x00FF00); // PCI
    
    if (acpi_init() == 0) {
        boot_log_add_hex("ACPI", "RSDP at", 0x58A6FF, 0xE8EAED, (uint64_t)rsdp_request.response->address, 0x3FB950);
        draw_early_progress(18, 0x00FF00); // ACPI
    } else {
        boot_log_add("ACPI", "No ACPI tables (degraded)", 0xF0883E, 0xE8EAED);
        draw_early_progress(18, 0xF0883E);
    }
    
    gpu_init();
    boot_log_add("GPU", "Graphics pipeline initialized", 0x58A6FF, 0xE8EAED);
    draw_early_progress(19, 0x00FF00); // GPU

    __asm__ volatile("sti");

    net_init();
    draw_early_progress(20, 0x00FF00); // Network
    
    init_mouse();
    mouse_set_bounds(get_fb_width(), get_fb_height());
    boot_log_add("IN", "HID mouse initialized", 0x58A6FF, 0xE8EAED);
    draw_early_progress(21, 0x00FF00); // Mouse
    
    vfs_init();
    draw_early_progress(22, 0x00FF00); // VFS
    
    ide_init();
    boot_log_add_hex("IDE", "Primary bus I/O base", 0x58A6FF, 0xE8EAED, (uint64_t)0x1F0, 0x3FB950);
    draw_early_progress(23, 0x00FF00); // IDE
    
    serial_puts("[KERNEL] mounting ramfs...\n");
    vfs_mount("/", ramfs_init());
    draw_early_progress(24, 0x00FF00); // Mount
    uint32_t dev_count = storage_get_device_count();
    for (uint32_t i = 0; i < dev_count && i < 6; i++) {
        boot_log_add_hex("BLK", "Storage device found", 0xBC8CFF, 0xE8EAED, (uint64_t)i, 0x3FB950);
        draw_early_progress(25 + i, 0x00FF00);
        block_device_t* dev = storage_get_device(i);
        boot_log_add_hex("BLK", "  sector count", 0xBC8CFF, 0xE8EAED, dev->size_sectors, 0xF0F6FC);
        vfs_node_t* partition = fat32_init(dev, 0);
        if (partition) {
            vfs_mount(dev->name, partition);
            boot_log_add("BLK", "  partition mounted", 0xBC8CFF, 0x3FB950);
        }
    }
    serial_puts("[KERNEL] creating background worker...\n");
    create_task(background_worker, "Worker");
    serial_puts("[KERNEL] background worker created\n");
    
    draw_early_progress(31, 0x00FF00); // GUI
    
    extern void set_splash_mode(bool mode);
    set_splash_mode(false);
    extern void clear_screen(void);
    clear_screen();
    extern void draw_premium_wallpaper(void);
    draw_premium_wallpaper();
    swap_buffers();

    serial_puts("[KERNEL] Starting GUI...\n");
    start_gui();    // Boot to GUI

    while(1) {
        __asm__ volatile("hlt");
    }
}

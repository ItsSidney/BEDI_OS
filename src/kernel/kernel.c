// kernel.c: BEDI OS Kernel entry point
#include "../../include/limine.h"
#include "../../include/commands.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/gui.h"
#include "../../include/idt.h"
#include "../filesystem/filesystem.h"

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
    while (*s) {
        while ((inb_k(0x3F8+5) & 0x20) == 0);
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
    0xf9562b2d5c95a6c8, 0x6a7b384944536bdc, 2
};

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

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

void print_banner() {
    int color = (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE;
    int yellow = (VGA_COLOR_BLUE << 4) | VGA_COLOR_YELLOW;
    int white = (VGA_COLOR_BLUE << 4) | VGA_COLOR_WHITE;
    int green = (VGA_COLOR_BLUE << 4) | VGA_COLOR_GREEN;

    for (int i = 0; i < 80 * 25; i++) {
        print_char_at(' ', color, i % 80, i / 80);
    }
    set_cursor(0, 0);

    // Border
    for (int i = 0; i < 80; i++) {
        print_char_at(205, yellow, i, 0);
        print_char_at(205, yellow, i, 24);
    }
    for (int i = 0; i < 25; i++) {
        print_char_at(186, yellow, 0, i);
        print_char_at(186, yellow, 79, i);
    }
    print_char_at(201, yellow, 0, 0);
    print_char_at(187, yellow, 79, 0);
    print_char_at(200, yellow, 0, 24);
    print_char_at(188, yellow, 79, 24);

    // ASCII Art
    set_cursor(25, 3);
    print_string_color("  ____  ______ _____ _____ ", white);
    set_cursor(25, 4);
    print_string_color(" |  _ \\|  ____|  __ \\_   _|", white);
    set_cursor(25, 5);
    print_string_color(" | |_) | |__  | |  | || |  ", white);
    set_cursor(25, 6);
    print_string_color(" |  _ <|  __| | |  | || |  ", white);
    set_cursor(25, 7);
    print_string_color(" | |_) | |____| |__| || |_ ", white);
    set_cursor(25, 8);
    print_string_color(" |____/|______|_____/_____|", white);

    set_cursor(22, 11);
    print_string_color("B E D I O S   N E X T   G E N E R A T I O N", yellow);

    set_cursor(20, 14);
    print_string_color("S Y S T E M   I N I T I A L I Z I N G", white);

    int bar_y = 16;
    set_cursor(20, bar_y);
    print_string_color("[", white);
    set_cursor(59, bar_y);
    print_string_color("]", white);

    for (int i = 0; i < 38; i++) {
        print_char_at(219, green, 21 + i, bar_y);
        set_cursor(37, 18);
        char percent[5];
        int p = (i * 100) / 37;
        percent[0] = (p / 10) + '0';
        percent[1] = (p % 10) + '0';
        percent[2] = '%';
        percent[3] = 0;
        print_string_color(percent, white);
        delay(10000000);
    }

    set_cursor(10, 21);
    print_string_color("[TIP] use 'help' to view commands", yellow);
    set_cursor(24, 23);
    print_string_color("Press any key to start BEDI OS...", white);

    get_key();
    clear_screen();
}

void kmain() {
    serial_puts("[BEDI] kmain()\n");
    clear_screen();

    init_idt();
    init_filesystem();

    start_gui(); // Boot directly to GUI

    // Fallback shell if GUI exits
    char command_buffer[256];
    int buffer_index = 0;
    print_prompt();

    while(1) {
        char key = get_key();
        if (key != 0) {
            if (key == KEY_UP || key == KEY_DOWN || key == KEY_LEFT || key == KEY_RIGHT || key == KEY_ESC) {
                continue;
            }

            if (key == '\n') {
                command_buffer[buffer_index] = 0;
                execute_command(command_buffer);
                buffer_index = 0;
                print_prompt();
            } else if (key == '\t') {
                handle_tab(command_buffer, &buffer_index);
            } else if (key == '\b') {
                if (buffer_index > 0) { buffer_index--; print_backspace(); }
            } else if (key >= 32 && key <= 126 && buffer_index < 255) {
                command_buffer[buffer_index++] = key;
                char str[2] = {key, 0};
                print_string(str);
            }
            swap_buffers(); // Update display
        }
    }
}

extern void init_fpu();

void _start(void) {
    serial_init();
    serial_puts("[BEDI] _start()\n");

    init_fpu();

    if (limine_base_revision[2] != 0) {
        serial_puts("[BEDI] FATAL: base revision\n");
        hcf();
    }

    if (framebuffer_request.response == 0 || framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("[BEDI] FATAL: no framebuffer\n");
        hcf();
    }

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

    serial_puts("[BEDI] FB init done\n");
    kmain();
    hcf();
}

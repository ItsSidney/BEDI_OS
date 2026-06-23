#include "drivers/video/framebuffer.h"
#include "gui/font.h"
#include "kernel/log.h"
#include <string.h>

static uint32_t* fb_addr = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;

static uint8_t fb_red_shift = 16;
static uint8_t fb_green_shift = 8;
static uint8_t fb_blue_shift = 0;

#define INTERNAL_STRIDE 1920
#define MAX_HEIGHT 1080
static uint32_t back_buffer[INTERNAL_STRIDE * MAX_HEIGHT];

static int cursor_x = 0;
static int cursor_y = 0;
static bool splash_mode = true;

static const uint32_t vga_palette[16] = {
    0x000000, 0x1F6FEB, 0x3FB950, 0x39D2C0, 0xF85149, 0xBC8CFF, 0xF0883E, 0x8B949E,
    0x484F58, 0x58A6FF, 0x56D364, 0x79C0FF, 0xFFA198, 0xD2A8FF, 0xE3B341, 0xF0F6FC
};

static inline uint32_t rgb_to_pixel(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    if (fb_red_shift == 0 && fb_green_shift == 0 && fb_blue_shift == 0) return (r << 16) | (g << 8) | b;
    return (r << fb_red_shift) | (g << fb_green_shift) | (b << fb_blue_shift);
}

static inline uint32_t vga_to_pixel(int vga_idx) { return rgb_to_pixel(vga_palette[vga_idx & 0x0F]); }

static void draw_rect(int x, int y, int w, int h, uint32_t col) {
    for(int ry=y; ry<y+h; ry++)
        for(int rx=x; rx<x+w; rx++)
            if(rx>=0 && rx<(int)fb_width && ry>=0 && ry<(int)fb_height)
                back_buffer[ry * INTERNAL_STRIDE + rx] = col;
}

void init_framebuffer(uint32_t* address, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp,
                      uint8_t red_shift, uint8_t green_shift, uint8_t blue_shift) {
    fb_addr = address;
    fb_width = (width > INTERNAL_STRIDE) ? INTERNAL_STRIDE : width;
    fb_height = (height > MAX_HEIGHT) ? MAX_HEIGHT : height;
    fb_pitch = pitch;
    fb_bpp = bpp;
    fb_red_shift = red_shift;
    fb_green_shift = green_shift;
    fb_blue_shift = blue_shift;
    
    uint32_t bg = vga_to_pixel(VGA_COLOR_BLACK);
    for (uint32_t i = 0; i < INTERNAL_STRIDE * MAX_HEIGHT; i++) back_buffer[i] = bg;
}

void swap_buffers() {
    if (!fb_addr) return;
    uint32_t* dest_row = fb_addr;
    uint32_t* src_row = back_buffer;
    for (uint32_t y = 0; y < fb_height; y++) {
        for (uint32_t x = 0; x < fb_width; x++) {
            dest_row[x] = src_row[x];
        }
        dest_row = (uint32_t*)((uintptr_t)dest_row + fb_pitch);
        src_row += INTERNAL_STRIDE;
    }
}

extern int gui_terminal_active;
extern void gui_terminal_print(const char* message);
extern void gui_terminal_clear(void);
extern void gui_terminal_backspace(void);

void set_splash_mode(bool mode) { splash_mode = mode; }

// ============ BOOT LOG ============
#define BOOT_LOG_MAX 80
#define BOOT_LINE_H 20

typedef struct {
    char tag[16];
    char msg[72];
    uint32_t tag_color;
    uint32_t msg_color;
} boot_entry_t;

static boot_entry_t boot_log[BOOT_LOG_MAX];
static int boot_log_count = 0;

static void boot_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (x < 0 || x + 8 > (int)fb_width || y < 0 || y + 16 > (int)fb_height) return;
    const uint8_t* glyph = &font8x16[(uint8_t)c * 16];
    for (int gy = 0; gy < 16; gy++) {
        uint8_t row = glyph[gy];
        int py = y + gy;
        for (int gx = 0; gx < 8; gx++) {
            int px = x + gx;
            back_buffer[py * INTERNAL_STRIDE + px] = (row & (0x80 >> gx)) ? fg : bg;
        }
    }
}

static void boot_draw_str(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str && x + 8 < (int)fb_width) {
        boot_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

void boot_log_add(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color) {
    if (boot_log_count < BOOT_LOG_MAX) {
        int i = 0;
        while (tag[i] && i < 15) { boot_log[boot_log_count].tag[i] = tag[i]; i++; }
        boot_log[boot_log_count].tag[i] = 0;
        i = 0;
        while (msg[i] && i < 71) { boot_log[boot_log_count].msg[i] = msg[i]; i++; }
        boot_log[boot_log_count].msg[i] = 0;
        boot_log[boot_log_count].tag_color = tag_color;
        boot_log[boot_log_count].msg_color = msg_color;
        boot_log_count++;
    }
}

void draw_boot_log(void) {
    if (fb_width == 0 || fb_height == 0) return;
    uint32_t bg = rgb_to_pixel(0x000000);
    uint32_t grey = rgb_to_pixel(0xAAAAAA);

    for (uint32_t i = 0; i < fb_width * fb_height; i++) back_buffer[i] = bg;

    int max_visible = (fb_height - 8) / BOOT_LINE_H;
    int start = boot_log_count - max_visible;
    if (start < 0) start = 0;

    int y = 4;
    for (int i = start; i < boot_log_count && y + 16 < (int)fb_height - 4; i++) {
        uint32_t fg_tag = rgb_to_pixel(boot_log[i].tag_color);
        uint32_t fg_msg = rgb_to_pixel(boot_log[i].msg_color);
        boot_draw_str(8, y, "[", grey, bg);
        boot_draw_str(16, y, boot_log[i].tag, fg_tag, bg);
        int x = 16 + (int)strlen(boot_log[i].tag) * 8;
        boot_draw_str(x, y, "]", grey, bg);
        boot_draw_str(x + 8, y, " ", grey, bg);
        boot_draw_str(x + 16, y, boot_log[i].msg, fg_msg, bg);
        y += BOOT_LINE_H;
    }

    swap_buffers();
}

void draw_splash_screen(int step) {
    (void)step;
    draw_boot_log();
}

void clear_terminal_cursor() {
    print_char_at(' ', (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE, cursor_x, cursor_y);
}

void clear_screen() {
    clear_terminal_cursor();
    if (gui_terminal_active) { gui_terminal_clear(); return; }
    uint32_t bg = vga_to_pixel(VGA_COLOR_BLACK);
    for (uint32_t i = 0; i < INTERNAL_STRIDE * MAX_HEIGHT; i++) back_buffer[i] = bg;
    cursor_x = 0; cursor_y = 0;
}

void scroll() {
    uint32_t row_h = (fb_height / MAX_ROWS);
    if (row_h == 0) return;
    uint32_t* src = back_buffer + (row_h * INTERNAL_STRIDE);
    uint32_t* dst = back_buffer;
    uint32_t copy_size = (MAX_ROWS - 1) * row_h * INTERNAL_STRIDE;
    for (uint32_t i = 0; i < copy_size; i++) dst[i] = src[i];
    uint32_t* last_row = back_buffer + ((MAX_ROWS - 1) * row_h * INTERNAL_STRIDE);
    uint32_t bg = vga_to_pixel(VGA_COLOR_BLACK);
    for (uint32_t i = 0; i < row_h * INTERNAL_STRIDE; i++) last_row[i] = bg;
    cursor_y = MAX_ROWS - 1;
}

void set_cursor(int x, int y) {
    if (x >= 0 && x < MAX_COLS) cursor_x = x;
    if (y >= 0 && y < MAX_ROWS) cursor_y = y;
}

void get_cursor(int* x, int* y) { *x = cursor_x; *y = cursor_y; }

uint32_t* get_fb_ptr() { return back_buffer; }
uint32_t get_fb_width() { return fb_width; }
uint32_t get_fb_height() { return fb_height; }
uint32_t gfx_get_stride() { return INTERNAL_STRIDE; }

void print_char_at(char character, int color, int x, int y) {
    if (x < 0 || x >= MAX_COLS || y < 0 || y >= MAX_ROWS) return;
    uint32_t cw = (fb_width / MAX_COLS);
    uint32_t ch = (fb_height / MAX_ROWS);
    if (cw == 0 || ch == 0) return;
    uint32_t fg = vga_to_pixel(color & 0x0F);
    uint32_t bg = vga_to_pixel((color >> 4) & 0x0F);
    const uint8_t* glyph = &font8x16[(uint8_t)character * 16];
    for (uint32_t gy = 0; gy < ch; gy++) {
        uint8_t line = glyph[gy >> 1];
        uint32_t* dst = back_buffer + (((y * ch) + gy) * INTERNAL_STRIDE) + (x * cw);
        for (uint32_t gx = 0; gx < cw; gx++) {
            dst[gx] = (line & (0x80 >> (gx >> 1))) ? fg : bg;
        }
    }
}

void print_string_color(const char* message, int color) {
    if (splash_mode) {
        extern void serial_puts(const char* s);
        serial_puts(message);
        return;
    }
    klog(message);
    
    extern int gui_running;
    if (gui_running) {
        if (gui_terminal_active) { gui_terminal_print(message); }
        return;
    }
    
    while (*message) {
        clear_terminal_cursor();
        char c = *message++;
        if (c == '\n') { cursor_x = 0; cursor_y++; }
        else { print_char_at(c, color, cursor_x, cursor_y); cursor_x++; }
        if (cursor_x >= MAX_COLS) { cursor_x = 0; cursor_y++; }
        while (cursor_y >= MAX_ROWS) scroll();
    }
}

void print_string(const char* message) { print_string_color(message, (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE); }

void print_char(char character, uint32_t color) {
    if (gui_terminal_active) {
        char s[2] = {character, 0};
        extern void gui_terminal_print(const char*);
        gui_terminal_print(s);
        return;
    }
    print_char_at(character, (int)color, cursor_x, cursor_y);
    cursor_x++;
    if (cursor_x >= MAX_COLS) { cursor_x = 0; cursor_y++; }
    while (cursor_y >= MAX_ROWS) scroll();
}

void print_backspace() {
    if (gui_terminal_active) { gui_terminal_backspace(); return; }
    clear_terminal_cursor();
    if (cursor_x > 0) cursor_x--;
    else if (cursor_y > 0) { cursor_y--; cursor_x = MAX_COLS - 1; }
    print_char_at(' ', (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE, cursor_x, cursor_y);
}

void draw_terminal_cursor() {
    if (gui_terminal_active) return;
    draw_box_vga(cursor_x, cursor_y, 1, 1, VGA_COLOR_WHITE);
}

void put_pixel(uint32_t x, uint32_t y, uint32_t rgb_color) {
    if (x >= fb_width || y >= fb_height) return;
    back_buffer[(y * INTERNAL_STRIDE) + x] = rgb_to_pixel(rgb_color);
}

void draw_box_vga(int x_grid, int y_grid, int w_grid, int h_grid, int vga_color) {
    uint32_t cw = (fb_width / MAX_COLS);
    uint32_t ch = (fb_height / MAX_ROWS);
    if (cw == 0 || ch == 0) return;
    uint32_t color = vga_to_pixel(vga_color);
    for (int y = y_grid * ch; y < (y_grid + h_grid) * ch; y++) {
        uint32_t* dst = back_buffer + (y * INTERNAL_STRIDE) + (x_grid * cw);
        for (int x = 0; x < w_grid * cw; x++) dst[x] = color;
    }
}

uint32_t* gfx_get_back_buffer() { return back_buffer; }
uint32_t gfx_get_fb_width() { return fb_width; }
uint32_t gfx_get_fb_height() { return fb_height; }
uint32_t gfx_rgb_to_pixel(uint32_t rgb) { return rgb_to_pixel(rgb); }

static uint32_t bg_buffer[INTERNAL_STRIDE * MAX_HEIGHT];
void cache_background() {
    for (uint32_t i = 0; i < fb_width * fb_height; i++) bg_buffer[i] = back_buffer[i];
}
void restore_background() {
    for (uint32_t i = 0; i < fb_width * fb_height; i++) back_buffer[i] = bg_buffer[i];
}

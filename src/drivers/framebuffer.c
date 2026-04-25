#include "../../include/framebuffer.h"
#include "../../include/font.h"

static uint32_t* fb_addr = 0;
static uint32_t fb_width = 0;
static uint32_t fb_height = 0;
static uint32_t fb_pitch = 0;
static uint32_t fb_bpp = 0;

static uint8_t fb_red_shift = 16;
static uint8_t fb_green_shift = 8;
static uint8_t fb_blue_shift = 0;

// Double Buffering
#define MAX_FB_WIDTH 1024
#define MAX_FB_HEIGHT 768
static uint32_t back_buffer[MAX_FB_WIDTH * MAX_FB_HEIGHT];
static uint32_t bg_buffer[MAX_FB_WIDTH * MAX_FB_HEIGHT];

static int cursor_x = 0;
static int cursor_y = 0;

static const uint32_t vga_palette[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static inline uint32_t rgb_to_pixel(uint32_t rgb) {
    return (((rgb >> 16) & 0xFF) << fb_red_shift) |
           (((rgb >> 8) & 0xFF) << fb_green_shift) |
           ((rgb & 0xFF) << fb_blue_shift);
}

static inline uint32_t vga_to_pixel(int idx) {
    return rgb_to_pixel(vga_palette[idx & 0x0F]);
}

void init_framebuffer(uint32_t* address, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp,
                      uint8_t red_shift, uint8_t green_shift, uint8_t blue_shift) {
    fb_addr = address;
    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bpp = bpp;
    fb_red_shift = red_shift;
    fb_green_shift = green_shift;
    fb_blue_shift = blue_shift;
    clear_screen();
}

void scroll() {
    uint32_t row_h = (fb_height / MAX_ROWS);
    uint32_t pixel_rows_to_copy = fb_height - row_h;
    
    // Shift pixels up
    for (uint32_t y = 0; y < pixel_rows_to_copy; y++) {
        uint32_t *src = back_buffer + ((y + row_h) * MAX_FB_WIDTH);
        uint32_t *dst = back_buffer + (y * MAX_FB_WIDTH);
        for (uint32_t x = 0; x < fb_width; x++) dst[x] = src[x];
    }
    
    // Clear last row
    uint32_t bg = vga_to_pixel(VGA_COLOR_BLACK);
    for (uint32_t y = pixel_rows_to_copy; y < fb_height; y++) {
        uint32_t *dst = back_buffer + (y * MAX_FB_WIDTH);
        for (uint32_t x = 0; x < fb_width; x++) dst[x] = bg;
    }
    
    if (cursor_y > 0) cursor_y--;
}

void draw_gradient_background() {
    for (uint32_t y = 0; y < fb_height; y++) {
        uint8_t r = (y * 51) / fb_height;
        uint8_t g = 0;
        uint8_t b = 51;
        uint32_t pixel = rgb_to_pixel((r << 16) | (g << 8) | b);
        uint32_t *dst = back_buffer + (y * MAX_FB_WIDTH);
        for (uint32_t x = 0; x < fb_width; x++) dst[x] = pixel;
    }
}

void swap_buffers() {
    if (!fb_addr) return;
    uint32_t ppr = fb_pitch / 4;
    for (uint32_t y = 0; y < fb_height; y++) {
        uint32_t *src = back_buffer + (y * MAX_FB_WIDTH);
        uint32_t *dst = fb_addr + (y * ppr);
        for (uint32_t x = 0; x < fb_width; x++) dst[x] = src[x];
    }
}

void fill_screen_vga(int vga_color) {
    uint32_t pixel = vga_to_pixel(vga_color);
    for (uint32_t i = 0; i < MAX_FB_WIDTH * MAX_FB_HEIGHT; i++) back_buffer[i] = pixel;
}

void draw_box_vga(int col, int row, int w, int h, int vga_color) {
    uint32_t x_start = (col * fb_width) / MAX_COLS;
    uint32_t x_end = ((col + w) * fb_width) / MAX_COLS;
    uint32_t y_start = (row * fb_height) / MAX_ROWS;
    uint32_t y_end = ((row + h) * fb_height) / MAX_ROWS;
    uint32_t pixel = vga_to_pixel(vga_color);
    for (uint32_t y = y_start; y < y_end && y < MAX_FB_HEIGHT; y++) {
        uint32_t *dst = back_buffer + (y * MAX_FB_WIDTH) + x_start;
        for (uint32_t x = 0; x < (x_end - x_start) && (x_start + x) < MAX_FB_WIDTH; x++) dst[x] = pixel;
    }
}

void cache_background() {
    for (uint32_t i = 0; i < MAX_FB_WIDTH * MAX_FB_HEIGHT; i++) bg_buffer[i] = back_buffer[i];
}

void restore_background() {
    for (uint32_t i = 0; i < MAX_FB_WIDTH * MAX_FB_HEIGHT; i++) back_buffer[i] = bg_buffer[i];
}

void clear_screen() {
    fill_screen_vga(VGA_COLOR_BLACK);
    set_cursor(0, 0);
    swap_buffers();
}

void set_cursor(int x, int y) { cursor_x = x; cursor_y = y; }

void print_char_at(char character, int color, int col, int row) {
    if (col < 0 || col >= MAX_COLS || row < 0 || row >= MAX_ROWS) return;
    uint32_t x_start = (col * fb_width) / MAX_COLS;
    uint32_t y_start = (row * fb_height) / MAX_ROWS;
    uint32_t cw = (fb_width / MAX_COLS);
    uint32_t ch = (fb_height / MAX_ROWS);
    uint32_t fg = vga_to_pixel(color & 0x0F);
    uint32_t bg = vga_to_pixel((color >> 4) & 0x0F);
    const uint8_t* glyph = &font8x16[(uint8_t)character * 16];
    for (uint32_t y = 0; y < ch; y++) {
        uint8_t line = glyph[(y * 16) / ch];
        uint32_t *dst = back_buffer + ((y_start + y) * MAX_FB_WIDTH) + x_start;
        for (uint32_t x = 0; x < cw; x++) dst[x] = (line & (0x80 >> ((x * 8) / cw))) ? fg : bg;
    }
}

void print_string_color(const char* message, int color) {
    while (*message) {
        char c = *message++;
        if (c == '\n') { cursor_x = 0; cursor_y++; }
        else { print_char_at(c, color, cursor_x, cursor_y); cursor_x++; }
        if (cursor_x >= MAX_COLS) { cursor_x = 0; cursor_y++; }
        while (cursor_y >= MAX_ROWS) scroll();
    }
}

void print_string(const char* message) { print_string_color(message, (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE); }

void print_backspace() {
    if (cursor_x > 0) cursor_x--;
    else if (cursor_y > 0) { cursor_y--; cursor_x = MAX_COLS - 1; }
    print_char_at(' ', (VGA_COLOR_BLACK << 4) | VGA_COLOR_WHITE, cursor_x, cursor_y);
}

void put_pixel(uint32_t x, uint32_t y, uint32_t rgb_color) {
    if (x >= fb_width || y >= fb_height) return;
    back_buffer[(y * MAX_FB_WIDTH) + x] = rgb_to_pixel(rgb_color);
}

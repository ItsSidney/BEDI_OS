#include "drivers/video/framebuffer.h"
#include "drivers/video/gpu.h"
#include "gui/font.h"
#include "kernel/log.h"
#include <gfx/splash_bmp.h>
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
        memcpy(dest_row, src_row, fb_width * sizeof(uint32_t));
        dest_row = (uint32_t*)((uintptr_t)dest_row + fb_pitch);
        src_row += INTERNAL_STRIDE;
    }
    gpu_present();
}

extern int gui_terminal_active;
extern void gui_terminal_print(const char* message);
extern void gui_terminal_clear(void);
extern void gui_terminal_backspace(void);

void set_splash_mode(bool mode) { splash_mode = mode; }

// ============ BOOT LOG ============
#define BOOT_LOG_MAX 128
#define BOOT_HEX_MAX 24  // max hex chars to append

typedef struct {
    char tag[16];
    char msg[72];
    uint32_t tag_color;
    uint32_t msg_color;
    int has_hex;
    uint64_t hex_value;
    int hex_color;          // -1 = use msg_color, else override
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

static int boot_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

static void boot_draw_hex(uint64_t value, int x, int y, uint32_t fg, uint32_t bg) {
    char buf[BOOT_HEX_MAX + 4];
    int len = 0;
    buf[len++] = '0';
    buf[len++] = 'x';
    // Print 16 hex digits
    for (int i = 15; i >= 0; i--) {
        int digit = (value >> (i * 4)) & 0xF;
        buf[len++] = (char)(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
    buf[len++] = 0;
    boot_draw_str(x, y, buf, fg, bg);
}

void boot_log_add(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color) {
    if (boot_log_count >= BOOT_LOG_MAX) return;
    int i = 0;
    while (tag[i] && i < 15) { boot_log[boot_log_count].tag[i] = tag[i]; i++; }
    boot_log[boot_log_count].tag[i] = 0;
    i = 0;
    while (msg[i] && i < 71) { boot_log[boot_log_count].msg[i] = msg[i]; i++; }
    boot_log[boot_log_count].msg[i] = 0;
    boot_log[boot_log_count].tag_color = tag_color;
    boot_log[boot_log_count].msg_color = msg_color;
    boot_log[boot_log_count].has_hex = 0;
    boot_log[boot_log_count].hex_value = 0;
    boot_log[boot_log_count].hex_color = -1;
    boot_log_count++;
}

void boot_log_add_hex(const char* tag, const char* msg, uint32_t tag_color, uint32_t msg_color,
                       uint64_t hex_value, int hex_color_override) {
    if (boot_log_count >= BOOT_LOG_MAX) return;
    int i = 0;
    while (tag[i] && i < 15) { boot_log[boot_log_count].tag[i] = tag[i]; i++; }
    boot_log[boot_log_count].tag[i] = 0;
    i = 0;
    while (msg[i] && i < 71) { boot_log[boot_log_count].msg[i] = msg[i]; i++; }
    boot_log[boot_log_count].msg[i] = 0;
    boot_log[boot_log_count].tag_color = tag_color;
    boot_log[boot_log_count].msg_color = msg_color;
    boot_log[boot_log_count].has_hex = 1;
    boot_log[boot_log_count].hex_value = hex_value;
    boot_log[boot_log_count].hex_color = hex_color_override;
    boot_log_count++;
}

void draw_boot_log(void) {
    draw_splash_screen(boot_log_count);
}

// ── Black & white splash screen ─────────────────────────────────────────
#define BW_BLACK 0x000000
#define BW_WHITE 0xFFFFFF
#define BW_LGRAY 0xCCCCCC
#define BW_MGRAY 0x888888
#define BW_DGRAY 0x333333
#define BW_DIM   0x1A1A1A

static void bw_draw_letter(int x, int y, int bs, const uint8_t* pat, int bw, uint32_t col) {
    for (int row = 0; row < 7; row++)
        for (int c = 0; c < bw; c++)
            if (pat[row] & (1 << (bw - 1 - c)))
                draw_rect(x + c * bs, y + row * bs, bs, bs, col);
}

static const uint8_t BW_B[7] = {0x3F,0x33,0x33,0x3F,0x33,0x33,0x3F};
static const uint8_t BW_E[7] = {0x3F,0x30,0x30,0x3F,0x30,0x30,0x3F};
static const uint8_t BW_D[7] = {0x3E,0x33,0x33,0x33,0x33,0x33,0x3E};
static const uint8_t BW_I[7] = {0x3F,0x0C,0x0C,0x0C,0x0C,0x0C,0x3F};

// 7x7 icon patterns for stage badges
static const uint8_t ICON_CPU[7] = {0x08,0x1C,0x22,0x2A,0x22,0x1C,0x08};
static const uint8_t ICON_MEM[7] = {0x3E,0x22,0x2A,0x2A,0x2A,0x22,0x3E};
static const uint8_t ICON_KRN[7] = {0x1C,0x22,0x2A,0x1C,0x2A,0x22,0x1C};
static const uint8_t ICON_SEC[7] = {0x3E,0x22,0x22,0x22,0x1C,0x08,0x08};
static const uint8_t ICON_HW[7]  = {0x3E,0x22,0x22,0x22,0x3E,0x08,0x1C};
static const uint8_t ICON_NET[7] = {0x22,0x14,0x08,0x14,0x22,0x00,0x00};
static const uint8_t ICON_STR[7] = {0x3E,0x22,0x2A,0x2A,0x22,0x22,0x3E};
static const uint8_t ICON_GUI[7] = {0x3E,0x22,0x3E,0x22,0x22,0x22,0x3E};

static void bw_draw_icon(int x, int y, int s, const uint8_t* pat, uint32_t col) {
    for (int row = 0; row < 7; row++)
        for (int c = 0; c < 7; c++)
            if (pat[row] & (1 << (6 - c)))
                draw_rect(x + c * s, y + row * s, s, s, col);
}

void draw_splash_screen(int step) {
    if (!splash_mode) return;
    if (fb_width == 0 || fb_height == 0) return;
    int w = (int)fb_width, h = (int)fb_height;
    uint32_t black = rgb_to_pixel(BW_BLACK);
    uint32_t white = rgb_to_pixel(BW_WHITE);

    static int banner_drawn = 0;
    static int banner_x = 0, banner_y = 0, out_h = 0;

    if (!banner_drawn) {
        for (uint32_t i = 0; i < (uint32_t)w * h; i++) back_buffer[i] = black;

        int bw = bedi_banner_w, bh = bedi_banner_h;
        int out_w = w;
        out_h = (bh * out_w) / bw;
        if (out_h < 40) out_h = 40;
        banner_x = (w - out_w) / 2;
        banner_y = (h - out_h) / 10;
        draw_bmp(banner_x, banner_y, bedi_banner_bmp, bedi_banner_bmp_len,
                 back_buffer, INTERNAL_STRIDE, w, h, 0, 0, out_w, out_h);
        banner_drawn = 1;
    }

    int init_y = banner_y + out_h + 22;
    int max_lines = (h - init_y - 10) / 18;
    if (max_lines > 48) max_lines = 48;
    if (max_lines < 1) max_lines = 1;

    int count = boot_log_count;
    if (count > max_lines) count = max_lines;
    int start = 0;
    if (count < max_lines) start = 0;
    else start = count - max_lines;

    for (int i = start; i < count; i++) {
        int ly = init_y + (i - start) * 18;
        if (ly + 16 > h - 4) break;
        int tx = 20;
        // Tag
        boot_draw_str(tx, ly, boot_log[i].tag, white, black);
        tx += 40;
        // Msg
        boot_draw_str(tx, ly, boot_log[i].msg, white, black);
        // Hex value on the same line after msg
        if (boot_log[i].has_hex) {
            tx += boot_strlen(boot_log[i].msg) * 8 + 10;
            if (tx + 20 > w - 10) tx = w - 20 * 8 - 10;
            boot_draw_hex(boot_log[i].hex_value, tx, ly, white, black);
        }
    }

    swap_buffers();
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
    
    extern void serial_puts(const char* s);
    serial_puts(message);
    
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

int fb_draw_bmp(int x, int y, const unsigned char* data, unsigned int len) {
    return draw_bmp(x, y, data, len, back_buffer, INTERNAL_STRIDE, (int)fb_width, (int)fb_height, 0, 0, 0, 0);
}

int fb_get_width(void) { return (int)fb_width; }
int fb_get_height(void) { return (int)fb_height; }
uint32_t* fb_get_back_buffer(void) { return back_buffer; }

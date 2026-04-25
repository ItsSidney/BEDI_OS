#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

// VGA 4-bit color indices
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

// Arrow key codes (standardized)
#define KEY_UP    0x48
#define KEY_DOWN  0x50
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_ESC   27

// Virtual console grid
#define MAX_ROWS 25
#define MAX_COLS 80

void init_framebuffer(uint32_t* fb_address, uint32_t width, uint32_t height, uint32_t pitch, uint32_t bpp,
                      uint8_t red_shift, uint8_t green_shift, uint8_t blue_shift);

void swap_buffers();
void set_mouse_pos(int x, int y);
void fill_screen_vga(int vga_color);
void draw_box_vga(int x, int y, int w, int h, int vga_color);
void draw_gradient_background();
void cache_background();
void restore_background();

void clear_screen();
void print_char_at(char character, int color, int x, int y);
void print_char(char character, uint32_t color);
void print_string_color(const char* message, int color);
void print_string(const char* message);
void print_backspace();
void set_cursor(int x, int y);

uint32_t get_fb_width();
uint32_t get_fb_height();
void put_pixel(uint32_t x, uint32_t y, uint32_t rgb_color);

#endif

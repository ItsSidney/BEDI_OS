#ifndef TERMINAL_APP_H
#define TERMINAL_APP_H

#include <stdint.h>

// Terminal buffer accessors for bdim / editor integration
void term_set_cursor(int x, int y);
void term_set_scroll(int scroll);
void term_clear(void);
void term_putc_at(int x, int y, char c, uint32_t color);
char term_getc_at(int x, int y);
void term_set_color_at(int x, int y, uint32_t color);
void term_set_editor_cursor(int x, int y);
int term_get_width(void);
int term_get_height(void);
void term_set_current_color(uint32_t c);

// Standard terminal output
void gui_terminal_print(const char* message);
void gui_terminal_backspace(void);
void gui_terminal_clear(void);
void gui_terminal_set_color(uint32_t c);

#endif

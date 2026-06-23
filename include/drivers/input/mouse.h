#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// Mouse button state flags
#define MOUSE_LEFT   0x01
#define MOUSE_RIGHT  0x02
#define MOUSE_MIDDLE 0x04

// Initialize PS/2 mouse
void init_mouse(void);

// Called from IRQ12 handler — processes mouse packets
void mouse_handler(void);

// State accessors
int mouse_get_x(void);
int mouse_get_y(void);
int mouse_get_buttons(void);

// Returns 1 if mouse has moved or button state changed since last call
int mouse_has_update(void);

// Set screen bounds for clamping
void mouse_set_bounds(int max_x, int max_y);

// Draw the mouse cursor onto the back buffer
void mouse_draw_cursor(void);

// Mouse sensitivity (1=low, 2=normal, 3=high)
void mouse_set_sensitivity(int level);
int mouse_get_sensitivity(void);

#endif

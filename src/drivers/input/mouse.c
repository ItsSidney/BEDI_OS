// ============================================================
//  BEDI OS — PS/2 Mouse Driver
//  Robust initialization compatible with QEMU, Boxes, VBox
//  Handles mouse IRQ12, packet parsing, and cursor rendering
// ============================================================
#include "drivers/input/mouse.h"
#include "drivers/video/gfx.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"  // for port_byte_in/out

// ── Mouse state ─────────────────────────────────────────────
static volatile int mouse_x = 512;
static volatile int mouse_y = 384;
static volatile int mouse_buttons = 0;
static volatile int mouse_updated = 0;

static int bound_x = 1024;
static int bound_y = 768;

// PS/2 mouse packet state machine
static volatile int mouse_cycle = 0;
static volatile uint8_t mouse_packet[3];

// ── I/O delay for hardware compatibility ────────────────────
static void io_delay(void) {
    // Short delay using port 0x80 (POST diagnostic port)
    // This ensures the PS/2 controller has time to process commands
    // Critical for compatibility with GNOME Boxes (TCG mode)
    port_byte_in(0x80);
    port_byte_in(0x80);
    port_byte_in(0x80);
    port_byte_in(0x80);
}

// ── PS/2 controller helpers ─────────────────────────────────
static void mouse_wait_input(void) {
    int timeout = 100000;
    while (timeout--) {
        if (!(port_byte_in(0x64) & 0x02)) return;
        io_delay();
    }
}

static void mouse_wait_output(void) {
    int timeout = 100000;
    while (timeout--) {
        if (port_byte_in(0x64) & 0x01) return;
        io_delay();
    }
}

static uint8_t mouse_read(void) {
    mouse_wait_output();
    return port_byte_in(0x60);
}

static void mouse_write(uint8_t data) {
    mouse_wait_input();
    port_byte_out(0x64, 0xD4);  // Tell controller: next byte goes to mouse
    io_delay();
    mouse_wait_input();
    port_byte_out(0x60, data);
    io_delay();
    
    // Read and discard ACK (0xFA) from the mouse
    mouse_read();
}

// ── Initialization ──────────────────────────────────────────
void init_mouse(void) {
    // 1. Flush controller buffer thoroughly
    int flush_count = 0;
    while ((port_byte_in(0x64) & 1) && flush_count < 100) {
        port_byte_in(0x60);
        io_delay();
        flush_count++;
    }

    // 2. Disable both devices first for clean init
    mouse_wait_input();
    port_byte_out(0x64, 0xAD); // Disable keyboard
    io_delay();
    mouse_wait_input();
    port_byte_out(0x64, 0xA7); // Disable mouse
    io_delay();
    
    // Flush again
    while (port_byte_in(0x64) & 1) { port_byte_in(0x60); io_delay(); }

    // 3. Read controller configuration byte
    mouse_wait_input();
    port_byte_out(0x64, 0x20);
    io_delay();
    mouse_wait_output();
    uint8_t config = port_byte_in(0x60);

    // Enable IRQ1 (keyboard, bit 0) and IRQ12 (mouse, bit 1)
    config |= 0x03;
    // Enable clocks for both ports (clear disable bits 4 and 5)
    config &= ~0x30;
    // Ensure translation is enabled (bit 6) for compatibility
    config |= 0x40;

    // Write back modified config
    mouse_wait_input();
    port_byte_out(0x64, 0x60);
    io_delay();
    mouse_wait_input();
    port_byte_out(0x60, config);
    io_delay();

    // 4. Enable both devices
    mouse_wait_input();
    port_byte_out(0x64, 0xAE); // Enable keyboard
    io_delay();
    mouse_wait_input();
    port_byte_out(0x64, 0xA8); // Enable mouse (auxiliary)
    io_delay();

    // 5. Perform interface test (command 0xA9)
    mouse_wait_input();
    port_byte_out(0x64, 0xA9);
    io_delay();
    mouse_wait_output();
    uint8_t test_result = port_byte_in(0x60);
    (void)test_result; // We proceed regardless — some VMs return wrong values

    // 6. Re-enable mouse after test (test may disable it)
    mouse_wait_input();
    port_byte_out(0x64, 0xA8);
    io_delay();
    
    // 7. Reset mouse
    mouse_write(0xFF); // Reset command
    // Mouse should respond with 0xFA (ACK), then 0xAA (self-test pass), then 0x00
    // But we already consumed the ACK in mouse_write, so read the remaining
    mouse_wait_output();
    port_byte_in(0x60); // 0xAA or timeout
    io_delay();
    mouse_wait_output();
    port_byte_in(0x60); // 0x00 or timeout
    io_delay();

    // 8. Set defaults and enable streaming
    mouse_write(0xF6); // Set default values
    mouse_write(0xF4); // Enable data streaming
    
    // 9. Clear any potential leftover bytes
    flush_count = 0;
    while ((port_byte_in(0x64) & 1) && flush_count < 100) {
        port_byte_in(0x60);
        io_delay();
        flush_count++;
    }

    mouse_cycle = 0;
}

// Mouse sensitivity: 1=low, 2=normal, 3=high
static int mouse_sensitivity = 2;

void mouse_set_sensitivity(int level) {
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    mouse_sensitivity = level;
}

int mouse_get_sensitivity(void) { return mouse_sensitivity; }

// ── IRQ12 Handler (called from interrupt stub) ──────────────
void mouse_handler(void) {
    uint8_t status = port_byte_in(0x64);

    // Verify output buffer is full (bit 0)
    if (!(status & 0x01)) return;

    uint8_t data = port_byte_in(0x60);

    switch (mouse_cycle) {
        case 0:
            // First byte: status — validate it has bit 3 set (always 1 for PS/2)
            if (data & 0x08) {
                mouse_packet[0] = data;
                mouse_cycle = 1;
            }
            // If bit 3 not set, discard and re-sync
            break;

        case 1:
            // Second byte: X movement
            mouse_packet[1] = data;
            mouse_cycle = 2;
            break;

        case 2:
            // Third byte: Y movement
            mouse_packet[2] = data;
            mouse_cycle = 0;

            // Check for overflow — discard packet if overflow
            if (mouse_packet[0] & 0xC0) break;

            // Parse buttons
            mouse_buttons = mouse_packet[0] & 0x07;

            // Parse X movement (signed)
            int dx = (int)mouse_packet[1];
            if (mouse_packet[0] & 0x10) dx |= 0xFFFFFF00;  // Sign extend

            // Parse Y movement (signed, inverted — PS/2 Y is inverted)
            int dy = (int)mouse_packet[2];
            if (mouse_packet[0] & 0x20) dy |= 0xFFFFFF00;  // Sign extend

            // Apply sensitivity multiplier
            if (mouse_sensitivity == 1) {
                dx = dx / 2;
                dy = dy / 2;
            } else if (mouse_sensitivity == 3) {
                dx = dx + dx / 2;
                dy = dy + dy / 2;
            }

            // Apply movement (Y is inverted for screen coords)
            mouse_x += dx;
            mouse_y -= dy;

            // Clamp to screen bounds
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= bound_x) mouse_x = bound_x - 1;
            if (mouse_y >= bound_y) mouse_y = bound_y - 1;

            mouse_updated = 1;
            break;
    }
}

// ── State accessors ─────────────────────────────────────────
int mouse_get_x(void) { return mouse_x; }
int mouse_get_y(void) { return mouse_y; }
int mouse_get_buttons(void) { return mouse_buttons; }

int mouse_has_update(void) {
    if (mouse_updated) {
        mouse_updated = 0;
        return 1;
    }
    return 0;
}

void mouse_set_bounds(int max_x, int max_y) {
    bound_x = max_x;
    bound_y = max_y;
    // Clamp current position
    if (mouse_x >= bound_x) mouse_x = bound_x - 1;
    if (mouse_y >= bound_y) mouse_y = bound_y - 1;
}

// ── Cursor rendering ────────────────────────────────────────
// 12x18 arrow cursor bitmap (1 = white fill, 2 = black outline, 0 = transparent)
static const uint8_t cursor_bitmap[18][12] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,1,1,1,1,2,0,0,0},
    {2,1,1,1,1,1,1,1,1,2,0,0},
    {2,1,1,1,1,1,1,1,1,1,2,0},
    {2,1,1,1,1,1,2,2,2,2,2,0},
    {2,1,1,1,2,1,2,0,0,0,0,0},
    {2,1,1,2,0,2,1,2,0,0,0,0},
    {2,1,2,0,0,2,1,2,0,0,0,0},
    {2,2,0,0,0,0,2,1,2,0,0,0},
    {2,0,0,0,0,0,2,1,2,0,0,0},
    {0,0,0,0,0,0,0,2,0,0,0,0},
};

#define CURSOR_W 12
#define CURSOR_H 18

void mouse_draw_cursor(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            uint8_t pixel = cursor_bitmap[row][col];
            if (pixel == 0) continue;

            int px = mx + col;
            int py = my + row;

            if (pixel == 1) {
                gfx_blend_pixel(px, py, 0xFFFFFF, 240);
            } else if (pixel == 2) {
                gfx_blend_pixel(px, py, 0x000000, 200);
            }
        }
    }
}

int mouse_get_wheel_delta(void) {
    return 0;
}

void mouse_clear_wheel_delta(void) {
}

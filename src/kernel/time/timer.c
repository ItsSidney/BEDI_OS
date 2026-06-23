#include "kernel/time/timer.h"
#include "drivers/input/keyboard.h"

volatile uint64_t timer_ticks = 0;

void timer_handler(void) {
    timer_ticks++;
}

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void sleep_ms(uint32_t ms) {
    // Detect if PIT timer is ticking (critical for real hardware)
    // On some machines the PIC may not be fully configured yet.
    // We poll for up to ~50ms in busy-loop to see if ticks advance.
    uint64_t start = timer_ticks;
    for (volatile long i = 0; i < 100000000L; i++) {
        if (timer_ticks != start) break;
    }

    if (timer_ticks != start) {
        // Timer is working - use efficient hlt-based sleep
        uint64_t end_ticks = timer_ticks + ms;
        while (timer_ticks < end_ticks) {
            __asm__ volatile("hlt");
        }
    } else {
        // Timer NOT ticking (real hardware PIT issue) - busy-wait fallback
        // ~500,000 iterations ≈ 1ms on a 2GHz CPU (reduced for faster boot)
        for (volatile long i = 0; i < (long)ms * 500000L; i++);
    }
}

uint32_t timer_get_ms(void) {
    return (uint32_t)timer_ticks;
}

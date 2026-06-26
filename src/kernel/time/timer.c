#include "kernel/time/timer.h"
#include "drivers/input/keyboard.h"
#include "drivers/time/rtc.h"

extern unsigned char read_cmos(unsigned char reg);

volatile uint64_t timer_ticks = 0;
volatile int timer_detected = 0;
uint32_t timer_hz = 1000;

void timer_handler(void) {
    timer_ticks++;
}

void init_timer(uint32_t freq) {
    uint32_t divisor = 1193180 / freq;
    port_byte_out(0x43, 0x36);
    port_byte_out(0x40, (uint8_t)(divisor & 0xFF));
    port_byte_out(0x40, (uint8_t)((divisor >> 8) & 0xFF));
    timer_calibrate();
}

void sleep_ms(uint32_t ms) {
    if (!timer_detected) {
        uint64_t start = timer_ticks;
        for (volatile long i = 0; i < 100000L; i++) {
            if (timer_ticks != start) break;
        }
        timer_detected = 1;
    }

    uint64_t end_ticks = timer_ticks + (uint64_t)ms * timer_hz / 1000;
    if (end_ticks == timer_ticks) end_ticks++;
    while (timer_ticks < end_ticks) {
        __asm__ volatile("hlt");
    }
}

uint32_t timer_get_ms(void) {
    return (uint32_t)timer_ticks;
}

void timer_calibrate(void) {
    // Use RTC seconds as a reference to calibrate the actual PIT tick rate.
    unsigned char before = read_cmos(0x00);
    uint64_t start = timer_ticks;
    while (read_cmos(0x00) == before);
    uint64_t elapsed = timer_ticks - start;
    if (elapsed > 100) {
        timer_hz = (uint32_t)(elapsed);
    }
}

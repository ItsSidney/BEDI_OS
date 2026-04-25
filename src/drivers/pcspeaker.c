#include "../../include/pcspeaker.h"

static int is_muted = 0;
static int volume = 100;
static uint32_t current_freq = 1000;
static int is_playing = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void play_sound(uint32_t nFrequence) {
    if (is_muted || nFrequence == 0) return;
    uint32_t Div = 1193180 / nFrequence;
    outb(0x43, 0xB6);
    outb(0x42, (uint8_t) (Div) );
    outb(0x42, (uint8_t) (Div >> 8));
    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) outb(0x61, tmp | 3);
}

void nosound() {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

void beep() {
    if (is_muted || is_playing) return;
    is_playing = 1;
    play_sound(current_freq);
    for (volatile int i = 0; i < 8000000; i++); // Slightly longer for stability
    nosound();
    is_playing = 0;
}

void set_mute(int m) { is_muted = m; if (m) nosound(); }
int get_mute() { return is_muted; }
void set_volume(int v) { volume = v; if (v == 0) is_muted = 1; else is_muted = 0; }
int get_volume() { return volume; }
void set_freq(uint32_t f) { if (f > 2000) f = 2000; if (f < 100) f = 100; current_freq = f; }
uint32_t get_freq() { return current_freq; }

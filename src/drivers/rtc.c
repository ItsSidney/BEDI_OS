#include "../../include/rtc.h"
#include "../../include/keyboard.h"

static int offset_h = 0;
static int offset_m = 0;

unsigned char read_cmos(unsigned char reg) {
    port_byte_out(0x70, reg);
    return port_byte_in(0x71);
}

int is_updating() {
    port_byte_out(0x70, 0x0A);
    return (port_byte_in(0x71) & 0x80);
}

void get_time(time_t* t) {
    while (is_updating());
    
    unsigned char second = read_cmos(0x00);
    unsigned char minute = read_cmos(0x02);
    unsigned char hour = read_cmos(0x04);
    unsigned char registerB = read_cmos(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    }

    t->second = (int)second;
    t->minute = (int)minute + offset_m;
    t->hour = (int)hour + offset_h;
    
    // Normalize
    while (t->minute >= 60) { t->minute -= 60; t->hour++; }
    while (t->minute < 0) { t->minute += 60; t->hour--; }
    while (t->hour >= 24) { t->hour -= 24; }
    while (t->hour < 0) { t->hour += 24; }
}

void set_time_offset(int h, int m) {
    offset_h = h;
    offset_m = m;
}

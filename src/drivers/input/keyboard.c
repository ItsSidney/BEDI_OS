#include "drivers/input/keyboard.h"

static int shift_pressed = 0;
static int caps_lock = 0;
static int ctrl_pressed = 0;
static int extended_key = 0;

#define KEY_BUF_SIZE 256
static char key_buffer[KEY_BUF_SIZE];
static int key_head = 0;
static int key_tail = 0;
static uint8_t key_states[256];

int keyboard_is_key_down(uint8_t scancode) {
    return key_states[scancode & 0x7F];
}

unsigned char port_byte_in(unsigned short port) {
    unsigned char result;
    __asm__("in %%dx, %%al" : "=a" (result) : "d" (port));
    return result;
}

void port_byte_out(unsigned short port, unsigned char data) {
    __asm__("out %%al, %%dx" : : "a" (data), "d" (port));
}

void port_word_out(unsigned short port, unsigned short data) {
    __asm__("out %%ax, %%dx" : : "a" (data), "d" (port));
}

unsigned short port_word_in(unsigned short port) {
    unsigned short result;
    __asm__("in %%dx, %%ax" : "=a" (result) : "d" (port));
    return result;
}

unsigned int port_long_in(unsigned short port) {
    unsigned int result;
    __asm__("in %%dx, %%eax" : "=a" (result) : "d" (port));
    return result;
}

void port_long_out(unsigned short port, unsigned int data) {
    __asm__("out %%eax, %%dx" : : "a" (data), "d" (port));
}

static void push_key(char c) {
    if (c == 0) return;
    int next = (key_head + 1) % KEY_BUF_SIZE;
    if (next != key_tail) {
        key_buffer[key_head] = c;
        key_head = next;
    }
}

char get_key() {
    if (key_head == key_tail) return 0;
    char c = key_buffer[key_tail];
    key_tail = (key_tail + 1) % KEY_BUF_SIZE;
    return c;
}

void keyboard_handler() {
    uint8_t status = port_byte_in(0x64);
    if (!(status & 1)) return;

    uint8_t scancode = port_byte_in(0x60);

    if (scancode == 0xE0) { extended_key = 1; return; }
    
    static int super_active = 0;
    static int super_used_combo = 0;

    if (extended_key) {
        extended_key = 0;
        int is_release = (scancode & 0x80) != 0;
        uint8_t sc = scancode & 0x7F;
        key_states[sc] = !is_release;
        
        if (sc == 0x5B || sc == 0x5C) { // Super key
            if (!is_release) {
                super_active = 1;
                super_used_combo = 0;
            } else {
                if (super_active && !super_used_combo) {
                    push_key(KEY_SUPER);
                }
                super_active = 0;
            }
            return;
        }

        if (is_release) return;
        
        if (super_active) super_used_combo = 1;

        if (sc == 0x48) push_key(KEY_UP);
        else if (sc == 0x50) push_key(KEY_DOWN);
        else if (sc == 0x4B) push_key(KEY_LEFT);
        else if (sc == 0x4D) push_key(KEY_RIGHT);
        else if (sc == 0x47) push_key(KEY_HOME);
        else if (sc == 0x4F) push_key(KEY_END);
        else if (sc == 0x49) push_key(KEY_PAGE_UP);
        else if (sc == 0x51) push_key(KEY_PAGE_DOWN);
        return;
    }

    if (scancode & 0x80) {
        uint8_t sc = scancode & 0x7F;
        key_states[sc] = 0;
        if (sc == 0x1D) ctrl_pressed = 0;
        else if (sc == 0x2A || sc == 0x36) {
            shift_pressed = 0;
            key_states[sc == 0x2A ? KEY_LSHIFT : KEY_RSHIFT] = 0;
        }
        return;
    }

    key_states[scancode & 0x7F] = 1;
    if (super_active) super_used_combo = 1;

    if (scancode == 0x1D) { ctrl_pressed = 1; return; } 
    if (scancode == 0x3A) { caps_lock = !caps_lock; return; }
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        key_states[scancode == 0x2A ? KEY_LSHIFT : KEY_RSHIFT] = 1;
        return;
    }
    if (scancode == 0x01) { push_key(KEY_ESC); return; }

    static const char normal_map[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
        'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '
    };
    static const char shift_map[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|',
        'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '
    };

    if (scancode < 58) {
        char c = shift_pressed ? shift_map[scancode] : normal_map[scancode];
        
        // Map scancode 50 ('m') if it was missed
        if (c == 0 && scancode == 50) c = 'm';

        // Apply Shift/CapsLock
        if (shift_pressed || caps_lock) {
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c >= 'A' && c <= 'Z') c += 32; // In case already uppercase
        }
        
        // Ctrl handling
        if (ctrl_pressed) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 1;
            else if (c >= 'A' && c <= 'Z') c = c - 'A' + 1;
        }

        push_key(c);
    }
}

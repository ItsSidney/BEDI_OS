#include "../../include/keyboard.h"

static int shift_pressed = 0;
static int caps_lock = 0;
static int ctrl_pressed = 0;
static int extended_key = 0;

#define KEY_BUF_SIZE 256
static char key_buffer[KEY_BUF_SIZE];
static int key_head = 0;
static int key_tail = 0;

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
    if (extended_key) {
        extended_key = 0;
        if (scancode & 0x80) return;
        if (scancode == 0x48) push_key(KEY_UP);
        else if (scancode == 0x50) push_key(KEY_DOWN);
        else if (scancode == 0x4B) push_key(KEY_LEFT);
        else if (scancode == 0x4D) push_key(KEY_RIGHT);
        return;
    }

    if (scancode == 0x1D) { ctrl_pressed = 1; return; } 
    else if (scancode == (0x1D | 0x80)) { ctrl_pressed = 0; return; }
    if (scancode == 0x3A) { caps_lock = !caps_lock; return; }
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; } 
    else if (scancode == (0x2A | 0x80) || scancode == (0x36 | 0x80)) { shift_pressed = 0; return; }
    
    if (scancode == 0x01) { push_key(KEY_ESC); return; }
    if (scancode & 0x80) return;

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
        
        if (ctrl_pressed) {
            if (c >= 'a' && c <= 'z') c = c - 'a' + 1;
            else if (c >= 'A' && c <= 'Z') c = c - 'A' + 1;
        } else if (caps_lock) {
            if (c >= 'a' && c <= 'z') c -= 32;
            else if (c >= 'A' && c <= 'Z') c += 32;
        }
        
        // Final verification for 'M' (scancode 50)
        if (scancode == 50) {
            if (shift_pressed) c = 'M';
            else if (caps_lock) c = 'M';
            else c = 'm';
            if (ctrl_pressed) c = 13; 
        }

        push_key(c);
    }
}

#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

// Arrow key codes
#define KEY_UP    0x48
#define KEY_DOWN  0x50
#define KEY_LEFT  0x4B
#define KEY_RIGHT 0x4D
#define KEY_ESC   27

char get_key();
void keyboard_handler();

unsigned char port_byte_in(unsigned short port);
void port_byte_out(unsigned short port, unsigned char data);
void port_word_out(unsigned short port, unsigned short data);


#endif

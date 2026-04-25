#ifndef PCSPEAKER_H
#define PCSPEAKER_H

#include <stdint.h>

void play_sound(uint32_t nFrequence);
void nosound();
void beep();
void set_mute(int m);
int get_mute();
void set_volume(int v);
int get_volume();
void set_freq(uint32_t f);
uint32_t get_freq();

// Musical Notes
#define NOTE_C3 130
#define NOTE_CS3 138
#define NOTE_D3 146
#define NOTE_DS3 155
#define NOTE_E3 164
#define NOTE_F3 174
#define NOTE_FS3 185
#define NOTE_G3 196
#define NOTE_GS3 207
#define NOTE_A3 220
#define NOTE_AS3 233
#define NOTE_B3 246

#define NOTE_C4 261
#define NOTE_CS4 277
#define NOTE_D4 293
#define NOTE_DS4 311
#define NOTE_E4 329
#define NOTE_F4 349
#define NOTE_FS4 370
#define NOTE_G4 392
#define NOTE_GS4 415
#define NOTE_A4 440
#define NOTE_AS4 466
#define NOTE_B4 493

#define NOTE_C5 523

#endif

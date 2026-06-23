#ifndef TIMER_H
#define TIMER_H
#include <stdint.h>

void init_timer(uint32_t freq);
void sleep_ms(uint32_t ms);
uint32_t timer_get_ms(void);

#endif

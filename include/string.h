#ifndef _STRING_H
#define _STRING_H

#include "kernel/lib/string.h"

/* Also include other string functions defined elsewhere in BEDI OS */
#include <stdint.h>

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int strlen(const char* s);
void strcpy(char* dest, const char* src);
int memcmp(const void* s1, const void* s2, size_t n);
void itoa(uint64_t n, char* s);
char* strchr(const char* s, int c);

#endif

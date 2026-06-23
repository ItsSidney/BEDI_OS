#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

void print_prompt();
void execute_command(char* input);
void handle_tab(char* buffer, int* index);
void print_utf8(const char *s);
void reboot();
void shutdown();

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
int strlen(const char* s);
void strcpy(char* dest, const char* src);
void itoa(uint64_t n, char* s);

#endif

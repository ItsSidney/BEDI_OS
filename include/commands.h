#ifndef COMMANDS_H
#define COMMANDS_H

void print_prompt();
void execute_command(char* input);
void handle_tab(char* buffer, int* index);
void print_utf8(const char *s);
void reboot();
void shutdown();

#endif

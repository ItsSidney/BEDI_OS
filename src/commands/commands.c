#include "../../include/commands.h"
#include "../../include/framebuffer.h"
#include "../../include/keyboard.h"
#include "../../include/calculator.h"
#include "../../include/editor.h"
#include "../../include/gui.h"
#include "../filesystem/filesystem.h"

static const char* commands[] = {"help", "about", "clear", "reboot", "shutdown", "apps", "games", "ls", "rm", "mkdir", "cd", "pwd", "cat", "touch", "rmdir", "calc", "guess", "edit", "gui", 0};

int strcmp(char* s1, char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n-- && *s1 && (*s1 == *s2)) { s1++; s2++; }
    return (n < 0) ? 0 : *(unsigned char*)s1 - *(unsigned char*)s2;
}

void reboot() { print_string("\nSystem Rebooting..."); port_byte_out(0x64, 0xFE); }
void shutdown() { print_string("\nSystem Shutting Down..."); port_word_out(0x604, 0x2000); __asm__("cli; hlt"); }
void print_prompt() {
    print_string("\n");
    char path[256];
    fs_pwd(path, 256);
    if (strcmp(path, "/") == 0) print_string_color("[ROOT]", VGA_COLOR_RED);
    else if (strcmp(path, "/USER") == 0) print_string_color("[USER]", VGA_COLOR_GREEN);
    else {
        char display[64]; int i = 0;
        while (path[i] != 0 && i < 63) {
            if (path[i] >= 'a' && path[i] <= 'z') display[i] = path[i] - 'a' + 'A';
            else display[i] = path[i];
            i++;
        }
        display[i] = 0;
        char* display_name = (display[0] == '/') ? display + 1 : display;
        print_string_color("[", VGA_COLOR_YELLOW);
        print_string_color(display_name, VGA_COLOR_YELLOW);
        print_string_color("]", VGA_COLOR_YELLOW);
    }
    print_string(" -> ");
}

void handle_tab(char* buffer, int* index) {
    for (int i = 0; commands[i] != 0; i++) {
        if (strncmp(buffer, commands[i], *index) == 0) {
            const char* match = commands[i];
            for (int j = *index; match[j] != 0; j++) {
                buffer[(*index)++] = match[j];
                char s[2] = {match[j], 0}; print_string(s);
            }
            return;
        }
    }
}

void execute_command(char* input) {
    if (strcmp(input, "help") == 0) {
        print_string("\n  +-----------------------------------------------+\n");
        print_string("  |              BEDIOS HELP MENU                 |\n");
        print_string("  +------------------+----------------------------+\n");
        print_string("  |  COMMAND         |  DESCRIPTION               |\n");
        print_string("  +------------------+----------------------------+\n");
        print_string("  |  help            |  Show this menu            |\n");
        print_string("  |  about           |  About BEDIOS              |\n");
        print_string("  |  ls              |  List files                |\n");
        print_string("  |  calc            |  Scientific Calculator     |\n");
        print_string("  |  guess           |  Guessing Game             |\n");
        print_string("  |  edit <file>     |  Text editor               |\n");
        print_string("  |  gui             |  Start Graphical UI        |\n");
        print_string("  |  clear           |  Clear the screen          |\n");
        print_string("  |  reboot          |  Restart the system        |\n");
        print_string("  |  shutdown        |  Power off the system      |\n");
        print_string("  +------------------+----------------------------+\n");
    } else if (strcmp(input, "clear") == 0) clear_screen();
    else if (strcmp(input, "reboot") == 0) reboot();
    else if (strcmp(input, "shutdown") == 0) shutdown();
    else if (strcmp(input, "about") == 0) {
        print_string("\n  BEDIOS 64-bit UEFI v1.0.0\n  Target: HP Elite x2 G2 / VirtualBox\n  Author: Sidney\n");
    } else if (strcmp(input, "calc") == 0) calculator();
    else if (strcmp(input, "ls") == 0) {
        print_string("\n  Files:\n");
        char file_list[512];
        if (fs_list(file_list, 512) > 0) { print_string("  "); print_string(file_list); }
        else print_string("  (no files)\n");
    } else if (strncmp(input, "rm ", 3) == 0) {
        if (fs_delete(input + 3) == 0) print_string("\n  Deleted file.\n");
        else print_string("\n  Error: Not found.\n");
    } else if (strncmp(input, "mkdir ", 6) == 0) {
        if (fs_mkdir(input + 6) == 0) print_string("\n  Directory created.\n");
    } else if (strncmp(input, "cd ", 3) == 0) {
        if (fs_cd(input + 3) != 0) print_string("\n  Error: Not found.\n");
    } else if (strncmp(input, "edit ", 5) == 0) {
        const char* filename = input + 5;
        if (filename[0] == 0) {
            print_string("\n  Usage: edit <filename>\n");
        } else {
            char buf[1024]; int p = 0;
            for(int i=0; i<1024; i++) buf[i] = 0;
            
            // Load existing
            p = fs_cat(filename, buf, 1024);
            if (p < 0) p = 0;

            print_string("\n  --- Terminal Editor: "); print_string(filename); print_string(" ---\n");
            print_string("  (Content below - ESC to Save & Exit)\n\n  ");
            
            // Display current content
            if (p > 0) {
                for (int i=0; i<p; i++) {
                    if (buf[i] == '\n') print_string("\n  ");
                    else { char s[2]={buf[i],0}; print_string(s); }
                }
            }

            while(1) {
                char k = get_key();
                if (k == 27) break;
                if (k != 0) {
                    if (k == '\b') {
                        if (p > 0) {
                            p--;
                            if (buf[p] == '\n') {
                                // Hard to handle multi-line backspace in simple terminal
                                // but we'll just remove it from buffer
                            } else {
                                print_backspace();
                            }
                        }
                    } else if (k == '\n') {
                        if (p < 1023) { buf[p++] = '\n'; print_string("\n  "); }
                    } else if (p < 1023) {
                        buf[p++] = k;
                        char s[2] = {k, 0};
                        print_string(s);
                    }
                } else {
                    for(volatile int i=0; i<5000; i++);
                }
            }
            buf[p] = 0;
            fs_delete(filename);
            int fd = fs_create(filename);
            if (fd >= 0) { fs_write(fd, buf, p); fs_close(fd); print_string("\n\n  File saved successfully.\n"); }
        }
    } else if (strncmp(input, "mkdir ", 6) == 0) {
        char path[256]; fs_pwd(path, 256); print_string("\n"); print_string(path); print_string("\n");
    } else if (strcmp(input, "pwd") == 0) {
        char path[256]; fs_pwd(path, 256); print_string("\n"); print_string(path); print_string("\n");
    } else if (strncmp(input, "cat ", 4) == 0) {
        char content[1024];
        if (fs_cat(input + 4, content, 1024) >= 0) { print_string("\n"); print_string(content); print_string("\n"); }
    } else if (strncmp(input, "touch ", 6) == 0) {
        fs_touch(input + 6);
    } else if (strcmp(input, "gui") == 0) start_gui();
    else if (input[0] != 0) {
        print_string("\n"); print_string_color("Error: Unknown command.\n", VGA_COLOR_RED);
    }
}

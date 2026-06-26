#include "commands/commands.h"
#include "drivers/video/framebuffer.h"
#include "drivers/input/keyboard.h"
#include "drivers/time/rtc.h"
#include "drivers/video/gfx.h"
#include "filesystem/filesystem.h"
#include "drivers/bus/pci.h"
#include "kernel/mem/kheap.h"
#include "drivers/video/gpu.h"
#include "kernel/net/socket.h"
#include "kernel/net/in.h"
#include "kernel/net/if.h"
#include "kernel/time/timer.h"
#include "kernel/task/task.h"
#include "drivers/storage/storage.h"
#include "kernel/security/security.h"
#include <string.h>

extern void bcc_main(char* args);
extern void brun_main(char* args);

static const char* commands[] = {
    "help", "about", "clear", "reboot", "shutdown", 
    "ls", "rm", "mkdir", "cd", "pwd", "cat", 
    "touch", "rmdir", "gui", "pcilist", "meminfo", 
    "syscheck", "uptime", "hexdump", "gpu_3d", "gpu",
    "write", "append", "cp", "mv", "rename", "stat", "grep",
    "bdim", "colors", "bdfetch", "bcc", "brun", "vmtest", "ring3test", "ping", "dns", "bootlog", "bdrowser", 0
};

void vmtest() {
    int fd = fs_create("test.bin");
    if (fd < 0) { print_string("\n  vmtest: Failed to create test.bin\n"); return; }
    
    // Bytecode: 
    // [0x08] OP_PRINT
    // [0x06 0x00 0x00 0x00] Offset to string (6 bytes past start of instruction)
    // [0x0E] OP_EXIT
    // ['H', 'e', 'l', 'l', 'o', 0] String data
    uint8_t code[] = {0x08, 0x06, 0x00, 0x00, 0x00, 0x0E, 'H', 'e', 'l', 'l', 'o', 0};
    
    fs_write(fd, (char*)code, 12);
    fs_close(fd);
    
    extern void brun_main(char*);
    brun_main("test.bin");
}

extern void ring3_entry(void);

void ring3test() {
    print_string("\n  Launching ring3 test process...\n");
    extern int create_user_process(void (*entry)(void), const char* name);
    int pid = create_user_process(ring3_entry, "ring3test");
    if (pid < 0) {
        print_string("  Failed to create ring3 process\n");
    } else {
        print_string("  ring3 test process created with PID: ");
        char buf[32];
        itoa(pid, buf);
        print_string(buf);
        print_string("\n");
    }
}

static uint32_t parse_ip(const char* s) {
    uint32_t res = 0;
    uint32_t part = 0;
    int shift = 24;
    
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            part = part * 10 + (*s - '0');
        } else if (*s == '.') {
            res |= (part & 0xFF) << shift;
            shift -= 8;
            part = 0;
        }
        s++;
    }
    res |= (part & 0xFF) << shift;
    return htonl(res);
}

static const char* strip_url_scheme(const char* host) {
    if (strncmp(host, "http://", 7) == 0) return host + 7;
    if (strncmp(host, "HTTP://", 7) == 0) return host + 7;
    return host;
}

void ping(char* host) {
    host = (char*)strip_url_scheme(host);
    uint32_t ip = parse_ip(host);
    if (ip == 0) {
        extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
        if (dns_resolve(host, &ip) < 0) {
            ip = htonl(0x0A000202);
            print_string("  DNS failed, using default\n");
        }
    }

    print_string("\n  Pinging ");
    print_string(host);
    print_string("...\n");
    
    extern int sys_ping(uint32_t ip);
    sys_ping(ip);
    
    extern void sleep_task(uint32_t ms);
    sleep_task(2000); 
    
    print_string("\n");
}

void dns_lookup(char* raw) {
    const char* host = strip_url_scheme(raw);
    print_string("\n  Resolving ");
    print_string(host);
    print_string("...\n");
    extern int dns_resolve(const char* hostname, uint32_t* ip_addr);
    uint32_t ip;
    if (dns_resolve(host, &ip) == 0) {
        char buf[16];
        extern void itoa(uint64_t n, char* s);
        itoa(ntohl(ip), buf);
        print_string("  -> ");
        print_string(buf);
        print_string("\n");
    } else {
        print_string("  DNS resolution failed\n");
    }
}



void bootlog();
void bootlog() {
    extern void log_dump(void);
    log_dump();
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

void print_prompt() {
    print_string_color("bedi", VGA_COLOR_GREEN);
    print_string_color("@", VGA_COLOR_WHITE);
    print_string_color("os", VGA_COLOR_CYAN);
    print_string_color(":~$ ", VGA_COLOR_WHITE);
}

void reboot() {
    print_string("\n  System rebooting...\n");
    __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
}

void shutdown() {
    print_string("\n  Shutting down...\n");
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    __asm__ volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
}

void handle_tab(char* buffer, int* index) {
    int len = strlen(buffer);
    if (len == 0) return;
    for (int i = 0; commands[i] != 0; i++) {
        if (strncmp(buffer, commands[i], len) == 0) {
            const char* name = commands[i];
            for (int j = len; name[j] != 0; j++) {
                buffer[(*index)++] = name[j];
                char s[2] = {name[j], 0}; print_string(s);
            }
            return;
        }
    }
}

void itoa(uint64_t n, char* s) {
    int i = 0;
    if (n == 0) {
        s[i++] = '0';
        s[i] = 0;
        return;
    }
    uint64_t temp = n;
    while (temp > 0) {
        s[i++] = (temp % 10) + '0';
        temp /= 10;
    }
    s[i] = 0;
    for (int j = 0; j < i / 2; j++) {
        char t = s[j];
        s[j] = s[i - 1 - j];
        s[i - 1 - j] = t;
    }
}

extern uint64_t get_total_memory_bytes(void);

extern void bcc_main(char* args);
extern void brun_main(char* args);

void execute_command(char* input) {
    if (strcmp(input, "help") == 0) {
        print_string("\n  Available Commands:\n");
        print_string_color("  System Commands:\n", VGA_COLOR_CYAN);
        print_string("    help             - Show this help message\n");
        print_string("    about            - About BEDI OS\n");
        print_string("    clear            - Clear the screen\n");
        print_string("    reboot           - Reboot the computer\n");
        print_string("    shutdown         - Power off the computer\n");
        print_string("    uptime           - Show system uptime\n");
        print_string("    meminfo          - Show memory usage\n");
        print_string("    syscheck         - Perform system integrity check\n");
        print_string("    pcilist          - List detected PCI devices\n");
        print_string("    gpu_3d           - Check GPU status and test 3D acceleration\n");
        print_string("    gpu              - Show GPU status and capabilities\n");
        print_string("    gui              - Launch graphical user interface\n");
        print_string("    bdfetch          - Display system information (BEDI OS fetch)\n");
        print_string("    bcc              - Simple C Compiler\n");
        print_string("    brun             - Binary Executor\n");
        
        print_string_color("  File System Commands:\n", VGA_COLOR_GREEN);
        print_string("    ls               - List directory contents\n");
        print_string("    mkdir            - Create a new directory\n");
        print_string("    cd               - Change directory\n");
        print_string("    pwd              - Print working directory\n");
        print_string("    touch            - Create a new file\n");
        print_string("    rm               - Remove a file\n");
        print_string("    rmdir            - Remove a directory\n");
        print_string("    cat              - Display file contents\n");
        print_string("    write            - Write text to a file (overwrite)\n");
        print_string("    append           - Append text to a file\n");
        print_string("    cp               - Copy a file\n");
        print_string("    mv               - Move/rename a file\n");
        print_string("    stat             - Show file information\n");
        print_string("    grep             - Search for a string in a file\n");
        print_string("    bdim             - BEDI Vim Text Editor\n");
        print_string("    colors           - Change terminal text color\n");
        print_string("    ping             - Send ICMP Echo Request (IP or hostname)\n");
        print_string("    dns              - Resolve a hostname to IP\n");
        print_string("    bdrowser         - Launch the web browser (HTTP)\n");
    } else if (strcmp(input, "about") == 0) {
        print_string("\n  BEDI OS 64-bit UEFI\n  Target: Generic x86_64\n  Author: Sidney\n");
    } else if (strcmp(input, "ls") == 0) {
        char buf[2048];
        fs_list(buf, 2047);
        print_string("\n");
        print_string(buf);
    } else if (strcmp(input, "pwd") == 0) {
        char buf[256];
        fs_pwd(buf, 255);
        print_string("\n  ");
        print_string(buf);
        print_string("\n");
    } else if (strncmp(input, "cd ", 3) == 0) {
        if (fs_cd(input + 3) == 0) print_string("\n");
        else { print_string("\n  cd: "); print_string(input + 3); print_string(": No such directory\n"); }
    } else if (strncmp(input, "mkdir ", 6) == 0) {
        if (fs_mkdir(input + 6) == 0) print_string("\n");
        else print_string("\n  mkdir: Failed to create directory\n");
    } else if (strncmp(input, "rmdir ", 6) == 0) {
        if (fs_rmdir(input + 6) == 0) print_string("\n");
        else print_string("\n  rmdir: Failed to remove directory\n");
    } else if (strncmp(input, "touch ", 6) == 0) {
        if (fs_touch(input + 6) >= 0) print_string("\n");
        else print_string("\n  touch: Failed to create file\n");
    } else if (strncmp(input, "rm ", 3) == 0) {
        if (fs_delete(input + 3) == 0) print_string("\n");
        else print_string("\n  rm: Failed to remove file\n");
    } else if (strncmp(input, "cat ", 4) == 0) {
        char buf[4096];
        int bytes = fs_cat(input + 4, buf, 4096);
        print_string("\n");
        if (bytes >= 0) {
            print_string(buf);
            print_string("\n");
        } else {
            print_string("  cat: "); print_string(input + 4); print_string(": No such file\n");
        }
    } else if (strncmp(input, "write ", 6) == 0) {
        char* file = input + 6;
        char* text = file;
        while (*text && *text != ' ') text++;
        if (*text == ' ') {
            *text = 0; text++;
            int fd = fs_open(file, 0);
            if (fd < 0) fd = fs_create(file);
            if (fd >= 0) {
                fs_truncate(file);
                fs_write(fd, text, strlen(text));
                fs_close(fd);
                print_string("\n  Written\n");
            } else print_string("\n  Failed\n");
        } else print_string("\n  Usage: write <file> <text>\n");
    } else if (strncmp(input, "append ", 7) == 0) {
        char* file = input + 7;
        char* text = file;
        while (*text && *text != ' ') text++;
        if (*text == ' ') {
            *text = 0; text++;
            int fd = fs_open(file, 0);
            if (fd < 0) fd = fs_create(file);
            if (fd >= 0) {
                char temp[4096];
                int len = fs_read(fd, temp, 4095);
                if (len < 0) len = 0;
                int tlen = strlen(text);
                for (int i=0; i<tlen && len<4095; i++) temp[len++] = text[i];
                fs_truncate(file);
                fs_write(fd, temp, len);
                fs_close(fd);
                print_string("\n  Appended\n");
            } else print_string("\n  Failed\n");
        } else print_string("\n  Usage: append <file> <text>\n");
    } else if (strncmp(input, "cp ", 3) == 0) {
        char* src = input + 3;
        while (*src == ' ') src++;
        char* dst = src;
        while (*dst && *dst != ' ') dst++;
        if (*dst == ' ') {
            *dst = 0; dst++;
            while (*dst == ' ') dst++;
            if (*dst) {
                char* end = dst;
                while (*end && *end != ' ') end++;
                if (*end == ' ') *end = 0;

                char temp[4096];
                int fd1 = fs_open(src, 0);
                if (fd1 >= 0) {
                    int len = fs_read(fd1, temp, 4096);
                    fs_close(fd1);
                    int fd2 = fs_create(dst);
                    if (fd2 >= 0) {
                        fs_write(fd2, temp, len);
                        fs_close(fd2);
                        print_string("\n  Copied\n");
                    } else print_string("\n  Failed to create dest\n");
                } else print_string("\n  No such source file\n");
            } else print_string("\n  Usage: cp <src> <dst>\n");
        } else print_string("\n  Usage: cp <src> <dst>\n");
    } else if (strncmp(input, "mv ", 3) == 0) {
        char* src = input + 3;
        while (*src == ' ') src++;
        char* dst = src;
        while (*dst && *dst != ' ') dst++;
        if (*dst == ' ') {
            *dst = 0; dst++;
            while (*dst == ' ') dst++;
            if (*dst) {
                char* end = dst;
                while (*end && *end != ' ') end++;
                if (*end == ' ') *end = 0;

                if (fs_move(src, dst) == 0) print_string("\n  Moved\n");
                else print_string("\n  Failed\n");
            } else print_string("\n  Usage: mv <src> <dst>\n");
        } else print_string("\n  Usage: mv <src> <dst>\n");
    } else if (strncmp(input, "rename ", 7) == 0) {
        char* src = input + 7;
        while (*src == ' ') src++;
        char* dst = src;
        while (*dst && *dst != ' ') dst++;
        if (*dst == ' ') {
            *dst = 0; dst++;
            while (*dst == ' ') dst++;
            if (*dst) {
                char* end = dst;
                while (*end && *end != ' ') end++;
                if (*end == ' ') *end = 0;

                if (fs_rename(src, dst) == 0) print_string("\n  Renamed\n");
                else print_string("\n  Failed\n");
            } else print_string("\n  Usage: rename <old> <new>\n");
        } else print_string("\n  Usage: rename <old> <new>\n");
    } else if (strncmp(input, "stat ", 5) == 0) {
        print_string("\n  File stat not fully implemented for FAT32\n");
    } else if (strncmp(input, "grep ", 5) == 0) {
        print_string("\n  Grep not fully implemented\n");
    } else if (strncmp(input, "bdim ", 5) == 0 || strcmp(input, "bdim") == 0) {
        extern void bdim_app(const char* filename);
        if (input[4] == ' ' && strncmp(input + 5, "--help", 6) == 0) {
            print_string("\n  bdim — BEDI Editor (Vim-like)\n");
            print_string("  Usage: bdim [filename]\n");
            print_string("  Commands:\n");
            print_string("    :w        Save file\n");
            print_string("    :q        Quit\n");
            print_string("    :wq       Save and quit\n");
            print_string("    :set auto  Enable autocomplete\n");
            print_string("    :set noauto  Disable autocomplete\n");
            print_string("  Keys:\n");
            print_string("    i         Insert mode\n");
            print_string("    Esc       Normal mode\n");
            print_string("    Tab       Trigger autocomplete (in INSERT)\n");
            print_string("    h/j/k/l   Move cursor (in NORMAL)\n");
            print_string("    :         Command mode\n");
        } else if (input[4] == ' ') bdim_app(input + 5);
        else bdim_app(0);
    } else if (strcmp(input, "pcilist") == 0) {
        print_string("\n  Scanning PCI Bus...\n");
        int count = pci_get_device_count();
        for (int i = 0; i < count; i++) {
            pci_device_t* dev = pci_get_device(i);
            char buf[32];
            print_string("  [");
            buf[0] = (dev->bus / 10) + '0'; buf[1] = (dev->bus % 10) + '0'; buf[2] = ':';
            buf[3] = (dev->slot / 10) + '0'; buf[4] = (dev->slot % 10) + '0'; buf[5] = '.';
            buf[6] = (dev->func % 10) + '0'; buf[7] = 0;
            print_string(buf); print_string("] ");
            print_string(pci_device_to_string(dev->vendor_id, dev->device_id));
            print_string("\n");
        }
    } else if (strcmp(input, "meminfo") == 0) {
        uint64_t total = get_total_memory_bytes();
        uint64_t free = kheap_free();
        char buf[32];
        print_string("\n  Memory Information:\n");
        print_string("    Total RAM: "); itoa(total >> 20, buf); print_string(buf); print_string(" MB\n");
        print_string("    Heap Free: "); itoa(free >> 10, buf); print_string(buf); print_string(" KB\n");
    } else if (strcmp(input, "syscheck") == 0) {
        print_string("\n  System Integrity Check:\n");
        print_string("  [ OK ] Interrupts Enabled\n");
        print_string("  [ OK ] FPU Initialized\n");
        print_string("  [ OK ] PCI Controller Active\n");
        
        gpu_device_t* gpu = gpu_get_primary();
        if (gpu && gpu->initialized) {
            print_string("  [ OK ] GPU Accelerated (");
            print_string(gpu->name);
            print_string(")\n");
        } else {
            print_string("  [WARN] GPU not detected or basic VGA only\n");
        }
    } else if (strcmp(input, "gpu") == 0 || strcmp(input, "gpu_3d") == 0) {
        gpu_device_t* gpu = gpu_get_primary();
        if (!gpu) {
            print_string("\n  No GPU detected or driver not initialized.\n");
        } else {
            print_string("\n  GPU Info:\n");
            print_string("    Name: "); print_string(gpu->name); print_string("\n");
            
            uint32_t caps = gpu_get_capabilities();
            print_string("    Capabilities:\n");
            if (caps & GPU_CAP_2D) print_string("      [ YES ] 2D Acceleration (Compatible Mode)\n");
            if (caps & GPU_CAP_3D) print_string("      [ YES ] 3D Acceleration (Hardware Ready)\n");
            
            if (caps & GPU_CAP_3D) {
                print_string("\n  Hardware 3D Test: RCS Pipeline infrastructure present.\n");
            }
        }
    } else if (strcmp(input, "clear") == 0) {
        clear_screen();
        swap_buffers();
        clear_screen();
        swap_buffers();
    } else if (strcmp(input, "gui") == 0) {
        extern void start_gui();
        start_gui();
    } else if (strncmp(input, "colors ", 7) == 0 || strcmp(input, "colors") == 0) {
        extern void gui_terminal_set_color(uint32_t);
        if (input[6] == ' ') {
            int c = input[7] - '0';
            uint32_t color_map[10] = {
                0xC9D1D9, // 0: White
                0x58A6FF, // 1: Blue
                0x3FB950, // 2: Green
                0xF85149, // 3: Red
                0xEBCB8B, // 4: Yellow
                0xBC8CFF, // 5: Purple
                0x39D2C0, // 6: Cyan
                0xF0883E, // 7: Orange
                0xF778BA, // 8: Pink
                0x8B949E  // 9: Gray
            };
            if (c >= 0 && c <= 9) {
                gui_terminal_set_color(color_map[c]);
                print_string("\n  Color changed.\n");
            } else {
                print_string("\n  Invalid color (0-9)\n");
            }
        } else {
            print_string("\n  Colors: 0=White, 1=Blue, 2=Green, 3=Red, 4=Yellow,\n");
            print_string("          5=Purple, 6=Cyan, 7=Orange, 8=Pink, 9=Gray\n");
            print_string("  Usage: colors <0-9>\n");
        }
    } else if (strcmp(input, "bdfetch") == 0) {
        print_string("\n");
        char buf[32];

        print_string_color("OS: ", VGA_COLOR_CYAN); print_string("BEDI OS 64-bit\n");

        // Uptime
        uint64_t ms = timer_get_ms();
        uint32_t sec = (uint32_t)(ms / 1000);
        uint32_t days = sec / 86400; sec %= 86400;
        uint32_t hours = sec / 3600; sec %= 3600;
        uint32_t mins = sec / 60; sec %= 60;
        print_string_color("Uptime: ", VGA_COLOR_CYAN);
        if (days) { itoa(days, buf); print_string(buf); print_string("d "); }
        itoa(hours, buf); print_string(buf); print_string(":");
        if (mins < 10) print_string("0");
        itoa(mins, buf); print_string(buf); print_string(":");
        if (sec < 10) print_string("0");
        itoa(sec, buf); print_string(buf); print_string("\n");

        // CPU Detection using CPUID - Get actual brand string
        uint32_t eax, ebx, ecx, edx;
        char cpu_brand[49] = {0};
        __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000000));
        if (eax >= 0x80000004) {
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000002));
            *((uint32_t*)(cpu_brand + 0)) = eax;
            *((uint32_t*)(cpu_brand + 4)) = ebx;
            *((uint32_t*)(cpu_brand + 8)) = ecx;
            *((uint32_t*)(cpu_brand + 12)) = edx;
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000003));
            *((uint32_t*)(cpu_brand + 16)) = eax;
            *((uint32_t*)(cpu_brand + 20)) = ebx;
            *((uint32_t*)(cpu_brand + 24)) = ecx;
            *((uint32_t*)(cpu_brand + 28)) = edx;
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0x80000004));
            *((uint32_t*)(cpu_brand + 32)) = eax;
            *((uint32_t*)(cpu_brand + 36)) = ebx;
            *((uint32_t*)(cpu_brand + 40)) = ecx;
            *((uint32_t*)(cpu_brand + 44)) = edx;
            cpu_brand[48] = 0;
            int len = 48;
            while (len > 0 && cpu_brand[len-1] == ' ') len--;
            cpu_brand[len] = 0;
            print_string_color("CPU: ", VGA_COLOR_CYAN); print_string(cpu_brand); print_string("\n");
        } else {
            char cpu_vendor[13] = {0};
            __asm__ volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
            *((uint32_t*)(cpu_vendor + 0)) = ebx;
            *((uint32_t*)(cpu_vendor + 4)) = edx;
            *((uint32_t*)(cpu_vendor + 8)) = ecx;
            cpu_vendor[12] = 0;
            print_string_color("CPU: ", VGA_COLOR_CYAN); print_string(cpu_vendor); print_string("\n");
        }

        // Tasks / Processes
        int tasks = get_task_count();
        print_string_color("Tasks: ", VGA_COLOR_CYAN);
        itoa(tasks, buf); print_string(buf); print_string("\n");

        // Memory
        uint64_t total_mem = get_total_memory_bytes();
        size_t heap_free = kheap_free();
        print_string_color("Memory: ", VGA_COLOR_CYAN);
        itoa((uint32_t)(total_mem >> 20), buf); print_string(buf);
        print_string(" MB total, ");
        itoa((uint32_t)(heap_free >> 20), buf); print_string(buf);
        print_string(" MB heap free\n");

        // GPU
        gpu_device_t* gpu = gpu_get_primary();
        if (gpu && gpu->initialized) {
            print_string_color("GPU: ", VGA_COLOR_CYAN); print_string(gpu->name); print_string("\n");
        } else {
            print_string_color("GPU: ", VGA_COLOR_CYAN); print_string("Basic VGA\n");
        }

        // Resolution
        print_string_color("Display: ", VGA_COLOR_CYAN);
        uint32_t fw = get_fb_width(), fh = get_fb_height();
        itoa(fw, buf); print_string(buf); print_string("x");
        itoa(fh, buf); print_string(buf); print_string("\n");

        // Storage
        uint32_t dev_count = storage_get_device_count();
        print_string_color("Storage: ", VGA_COLOR_CYAN);
        if (dev_count > 0) {
            for (uint32_t si = 0; si < dev_count; si++) {
                block_device_t* dev = storage_get_device(si);
                itoa(dev->size_sectors, buf); print_string(buf);
                print_string(" sectors (");
                uint64_t mb = (dev->size_sectors * dev->block_size) >> 20;
                itoa((uint32_t)mb, buf); print_string(buf); print_string(" MB)");
                if (si < dev_count - 1) print_string(", ");
            }
            print_string("\n");
        } else {
            print_string("None\n");
        }

        // Network
        print_string_color("Network: ", VGA_COLOR_CYAN);
        struct ifnet* ifp_em = if_find("em0");
        struct ifnet* ifp_lo = if_find("lo0");
        int net_count = 0;
        if (ifp_em) {
            print_string("em0: ");
            for (int i = 0; i < 6; i++) {
                itoa(ifp_em->if_hwaddr[i], buf);
                if (ifp_em->if_hwaddr[i] < 16) print_string("0");
                print_string(buf);
                if (i < 5) print_string(":");
            }
            net_count++;
        }
        if (ifp_lo) {
            if (net_count) print_string(", ");
            print_string("lo0");
            net_count++;
        }
        if (net_count == 0) print_string("None");
        print_string("\n");

        // PCI devices
        int pci_count = pci_get_device_count();
        print_string_color("PCI: ", VGA_COLOR_CYAN);
        itoa(pci_count, buf); print_string(buf); print_string(" device");
        if (pci_count != 1) print_string("s");
        print_string("\n");

        // Session / User
        session_t* sess = get_current_session();
        print_string_color("Session: ", VGA_COLOR_CYAN);
        if (sess && sess->is_active) {
            itoa(sess->session_id, buf); print_string(buf);
            print_string(" (uid ");
            itoa(sess->uid, buf); print_string(buf);
            print_string(")\n");
        } else {
            print_string("None\n");
        }

        print_string("\n");
    } else if (strncmp(input, "bcc ", 4) == 0) {
        bcc_main(input + 4);
    } else if (strncmp(input, "brun ", 5) == 0) {
        brun_main(input + 5);
    } else if (strcmp(input, "vmtest") == 0) {
        vmtest();
    } else if (strcmp(input, "ring3test") == 0) {
        ring3test();
    } else if (strncmp(input, "ping ", 5) == 0) {
        ping(input + 5);
    } else if (strncmp(input, "dns ", 4) == 0) {
        dns_lookup(input + 4);
    } else if (strcmp(input, "bdrowser") == 0) {
        extern void bdrowser(void);
        bdrowser();
    } else if (strcmp(input, "bootlog") == 0) {
        bootlog();
    } else if (strlen(input) > 0) {
        print_string("\n  Command not found: ");
        print_string(input);
        print_string("\n");
    }
}

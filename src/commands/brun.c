#include "commands/commands.h"
#include "commands/vm.h"
#include "kernel/mem/kheap.h"
#include "drivers/video/framebuffer.h"
#include "filesystem/filesystem.h"

static void signed_itoa(int32_t n, char* s) {
    if (n < 0) {
        *s++ = '-';
        n = (uint32_t)-(int32_t)n; // Safer negation for INT_MIN
    }
    itoa((uint64_t)n, s);
}

void brun_main(char* args) {
    if (!args || *args == 0) {
        print_string("Usage: brun <file.bin>\n");
        return;
    }
    while (*args == ' ') args++;

    int fd = fs_open(args, 0);
    if (fd < 0) { print_string("\n  brun: File not found: "); print_string(args); print_string("\n"); return; }
    
    int arg_len = strlen(args);
    if (arg_len >= 3 && strcmp(args + arg_len - 3, ".bc") == 0) {
        print_string("\n  brun: Cannot execute source file (.bc). Compile it first with 'bcc'.\n");
        fs_close(fd);
        return;
    }
    
    uint8_t* code = kmalloc(VM_MEM_SIZE);
    if (!code) { print_string("\n  brun: Out of memory (code)\n"); fs_close(fd); return; }
    
    int len = fs_read(fd, (char*)code, VM_MEM_SIZE);
    fs_close(fd);

    if (len <= 4) { print_string("\n  brun: Empty or invalid file\n"); kfree(code); return; }

    int32_t code_size = *(int32_t*)code;
    if (code_size < 0 || code_size > len - 4) {
        print_string("\n  brun: Invalid bytecode header\n");
        kfree(code);
        return;
    }
    
    int32_t* stack = kmalloc(VM_STACK_SIZE * sizeof(int32_t));
    int32_t* vars = kmalloc(VM_STACK_SIZE * sizeof(int32_t));
    if (!stack || !vars) {
        print_string("\n  brun: Out of memory (stack/vars)\n");
        if (stack) kfree(stack);
        if (vars) kfree(vars);
        kfree(code);
        return;
    }

    // Initialize vars to 0
    for (int i = 0; i < VM_STACK_SIZE; i++) vars[i] = 0;

    int sp = 0;
    int pc = 4; // Start after header

    while (pc < 4 + code_size) {
        uint8_t op = code[pc++];
        switch (op) {
            case OP_PUSH: {
                int32_t val = *(int32_t*)&code[pc];
                pc += 4;
                if (sp >= VM_STACK_SIZE) { print_string("\n  brun: Stack overflow\n"); goto cleanup; }
                stack[sp++] = val;
                break;
            }
            case OP_ADD: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = a + b;
                break;
            }
            case OP_SUB: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = a - b;
                break;
            }
            case OP_MUL: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = a * b;
                break;
            }
            case OP_DIV: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                if (b == 0) { print_string("\n  brun: Division by zero\n"); goto cleanup; }
                stack[sp++] = a / b;
                break;
            }
            case OP_DADD: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = a + b;
                break;
            }
            case OP_DSUB: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = a - b;
                break;
            }
            case OP_DMUL: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = (int32_t)(((int64_t)a * b) / 1000000);
                break;
            }
            case OP_DDIV: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                if (b == 0) { print_string("\n  brun: Division by zero\n"); goto cleanup; }
                stack[sp++] = (int32_t)(((int64_t)a * 1000000) / b);
                break;
            }
            case OP_EQ: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = (a == b);
                break;
            }
            case OP_NE: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = (a != b);
                break;
            }
            case OP_LT: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = (a < b);
                break;
            }
            case OP_GT: {
                if (sp < 2) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t b = stack[--sp];
                int32_t a = stack[--sp];
                stack[sp++] = (a > b);
                break;
            }
            case OP_LOAD: {
                int32_t addr = *(int32_t*)&code[pc];
                pc += 4;
                if (addr < 0 || addr >= VM_STACK_SIZE) { print_string("\n  brun: Invalid var address\n"); goto cleanup; }
                if (sp >= VM_STACK_SIZE) { print_string("\n  brun: Stack overflow\n"); goto cleanup; }
                stack[sp++] = vars[addr];
                break;
            }
            case OP_STORE: {
                int32_t addr = *(int32_t*)&code[pc];
                pc += 4;
                if (addr < 0 || addr >= VM_STACK_SIZE) { print_string("\n  brun: Invalid var address\n"); goto cleanup; }
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                vars[addr] = stack[--sp];
                break;
            }
            case OP_PRINT_STR: {
                int32_t data_offset = *(int32_t*)&code[pc];
                pc += 4;
                if (data_offset < 4 + code_size || data_offset >= len) {
                    print_string("\n  brun: Invalid string address\n");
                } else {
                    print_string((char*)&code[data_offset]);
                }
                break;
            }
            case OP_PRINT_VAR_STR: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t data_offset = stack[--sp];
                
                // Check if it's a pointer to the code segment or an external buffer
                if (data_offset >= 4 + code_size && data_offset < len) {
                    print_string((char*)&code[data_offset]);
                } else {
                    // Assume it's a raw pointer to an external buffer
                    print_string((char*)(uintptr_t)data_offset);
                }
                break;
            }
            case OP_PRINT_INT: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t val = stack[--sp];
                char buf[32];
                signed_itoa(val, buf);
                print_string(buf);
                break;
            }
            case OP_PRINT_DOUBLE: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t val = stack[--sp];
                char buf[32];
                if (val < 0) { print_string("-"); val = -val; }
                signed_itoa(val / 1000000, buf);
                print_string(buf);
                print_string(".");
                int32_t frac = (val % 1000000);
                if (frac < 0) frac = -frac;
                signed_itoa(frac, buf);
                int len = strlen(buf);
                for(int i=0; i<6-len; i++) print_string("0");
                print_string(buf);
                break;
            }
            case OP_PRINT_NL: {
                print_string("\n");
                break;
            }
            case OP_INPUT_INT: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t addr = stack[--sp];
                char buf[32];
                extern int gui_terminal_input(char* buf, int max_len);
                gui_terminal_input(buf, 31);
                vars[addr] = 0;
                for(int i=0; buf[i]; i++) vars[addr] = vars[addr] * 10 + (buf[i] - '0');
                break;
            }
            case OP_INPUT_DOUBLE: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t addr = stack[--sp];
                char buf[32];
                extern int gui_terminal_input(char* buf, int max_len);
                gui_terminal_input(buf, 31);
                int val = 0, frac = 0, decimal = 0, neg = 0, i = 0;
                if (buf[0] == '-') { neg = 1; i++; }
                while (buf[i]) {
                    if (buf[i] == '.') { decimal = 1; i++; continue; }
                    int digit = buf[i] - '0';
                    if (decimal) { frac = frac * 10 + digit; }
                    else { val = val * 10 + digit; }
                    i++;
                }
                int res = val * 1000000 + frac * 1000;
                vars[addr] = neg ? -res : res;
                break;
            }
            case OP_INPUT_STR: {
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                int32_t addr = stack[--sp];
                static char input_str_buf[128];
                extern int gui_terminal_input(char* buf, int max_len);
                gui_terminal_input(input_str_buf, 127);
                // Return address of our static buffer
                vars[addr] = (int32_t)(uintptr_t)input_str_buf;
                break;
            }
            case OP_JMP: {
                int32_t addr = *(int32_t*)&code[pc];
                if (addr < 4 || addr >= 4 + code_size) { print_string("\n  brun: Invalid jump address\n"); goto cleanup; }
                pc = addr;
                break;
            }
            case OP_JZ: {
                int32_t addr = *(int32_t*)&code[pc];
                pc += 4;
                if (sp < 1) { print_string("\n  brun: Stack underflow\n"); goto cleanup; }
                if (stack[--sp] == 0) {
                    if (addr < 4 || addr >= 4 + code_size) { print_string("\n  brun: Invalid jump address\n"); goto cleanup; }
                    pc = addr;
                }
                break;
            }
            case OP_EXIT: {
                goto cleanup;
            }
            case OP_HALT:
                goto cleanup;
            default:
                print_string("\n  brun: Unknown opcode: ");
                char op_buf[4]; itoa(op, op_buf); print_string(op_buf); print_string("\n");
                goto cleanup;
        }
    }

cleanup:
    if (code) kfree(code);
    if (stack) kfree(stack);
    if (vars) kfree(vars);
}

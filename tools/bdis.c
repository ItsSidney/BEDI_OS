#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

const char* opcode_names[] = {
    "PUSH", "POP", "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV",
    "EQ", "NE", "LT", "GT", "PRINT_STR", "PRINT_VAR_STR",
    "PRINT_INT", "PRINT_DOUBLE", "INPUT_INT", "INPUT_DOUBLE", "INPUT_STR",
    "DADD", "DSUB", "DMUL", "DDIV", "PRINT_NL", "PLACEHOLDER",
    "JMP", "JZ", "CALL", "RET", "EXIT", "HALT"
};

int has_operand(uint8_t op) {
    if (op == 0 || op == 2 || op == 3 || op == 12 || op == 25 || op == 26 || op == 27) return 1;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("Usage: %s <file.bin>\n", argv[0]); return 1; }
    FILE* f = fopen(argv[1], "rb");
    if (!f) { perror("fopen"); return 1; }
    
    int32_t code_size;
    fread(&code_size, 4, 1, f);
    
    uint8_t* code = malloc(code_size);
    fread(code, 1, code_size, f);
    
    int pc = 0;
    while (pc < code_size) {
        uint8_t op = code[pc++];
        printf("%04d: %s", pc - 1, opcode_names[op]);
        if (has_operand(op)) {
            int32_t arg = *(int32_t*)&code[pc];
            printf(" %d", arg);
            pc += 4;
        }
        printf("\n");
    }
    
    fclose(f);
    free(code);
    return 0;
}

#include "commands/commands.h"
#include "commands/vm.h"
#include "filesystem/filesystem.h"
#include "kernel/mem/kheap.h"
#include "drivers/video/framebuffer.h"

// Tokenizer State
static char* src;
static char* src_start;
static int line_num = 1;
static char tok_sval[64];
static int tok_ival;
static int error_occurred = 0;

typedef enum { 
    TOK_INT, TOK_DOUBLE_TYPE, TOK_BOOL_TYPE, TOK_TRUE, TOK_FALSE, TOK_VOID, TOK_STRING_TYPE, TOK_MAIN, TOK_ID, TOK_INPUT, TOK_NUM, TOK_DOUBLE_NUM, TOK_STRING, 
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, 
    TOK_SEMICOLON, TOK_ASSIGN, TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, 
    TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_AND, TOK_OR, TOK_INC, TOK_DEC, TOK_COMMA, TOK_EOF, TOK_ERR,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_RETURN, TOK_EXIT
} tok_t;

static tok_t curr_tok;

static void error(const char* msg) {
    error_occurred = 1;
    print_string("\n  bcc error [line ");
    char buf[16]; itoa(line_num, buf); print_string(buf);
    print_string("]: ");
    print_string(msg);
    print_string("\n");
}

static void next_tok() {
    while (*src == ' ' || *src == '\n' || *src == '\r' || *src == '\t') {
        if (*src == '\n') line_num++;
        src++;
    }
    if (*src == '/' && *(src + 1) == '/') {
        while (*src != '\n' && *src != '\0') src++;
        next_tok();
        return;
    }
    if (*src == '\0') { curr_tok = TOK_EOF; return; }
    if (*src == '"') {
        src++; int i = 0; 
        while (*src != '"' && *src != '\0' && i < 63) {
            if (*src == '\\' && *(src+1) == 'n') { tok_sval[i++] = '\n'; src += 2; }
            else { tok_sval[i++] = *src++; }
        }
        tok_sval[i] = '\0'; 
        if (*src == '"') src++;
        curr_tok = TOK_STRING; 
        return;
    }
    if (*src >= '0' && *src <= '9') {
        int val = 0;
        while (*src >= '0' && *src <= '9') val = val * 10 + (*src++ - '0');
        if (*src == '.') {
            src++;
            int frac = 0;
            int divisor = 1;
            while (*src >= '0' && *src <= '9') {
                frac = frac * 10 + (*src++ - '0');
                divisor *= 10;
            }
            if (divisor == 10) frac *= 100000;
            else if (divisor == 100) frac *= 10000;
            else if (divisor == 1000) frac *= 1000;
            else if (divisor >= 1000000) frac /= (divisor / 1000000);
            tok_ival = val * 1000000 + frac;
            curr_tok = TOK_DOUBLE_NUM; return;
        }
        tok_ival = val;
        curr_tok = TOK_NUM; return;
    }
    if ((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || *src == '_') {
        int i = 0; while (((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src >= '0' && *src <= '9') || *src == '_') && i < 63) tok_sval[i++] = *src++;
        tok_sval[i] = '\0';
        if (strcmp(tok_sval, "int") == 0) curr_tok = TOK_INT;
        else if (strcmp(tok_sval, "double") == 0) curr_tok = TOK_DOUBLE_TYPE;
        else if (strcmp(tok_sval, "bool") == 0) curr_tok = TOK_BOOL_TYPE;
        else if (strcmp(tok_sval, "true") == 0) { curr_tok = TOK_TRUE; tok_ival = 1; }
        else if (strcmp(tok_sval, "false") == 0) { curr_tok = TOK_FALSE; tok_ival = 0; }
        else if (strcmp(tok_sval, "input") == 0) curr_tok = TOK_INPUT;
        else if (strcmp(tok_sval, "string") == 0) curr_tok = TOK_STRING_TYPE;
        else if (strcmp(tok_sval, "void") == 0) curr_tok = TOK_VOID;
        else if (strcmp(tok_sval, "main") == 0) curr_tok = TOK_MAIN;
        else if (strcmp(tok_sval, "if") == 0) curr_tok = TOK_IF;
        else if (strcmp(tok_sval, "else") == 0) curr_tok = TOK_ELSE;
        else if (strcmp(tok_sval, "while") == 0) curr_tok = TOK_WHILE;
        else if (strcmp(tok_sval, "for") == 0) curr_tok = TOK_FOR;
        else if (strcmp(tok_sval, "return") == 0) curr_tok = TOK_RETURN;
        else if (strcmp(tok_sval, "exit") == 0) curr_tok = TOK_EXIT;
        else curr_tok = TOK_ID;
        return;
    }
    switch (*src++) {
        case '(': curr_tok = TOK_LPAREN; break;
        case ')': curr_tok = TOK_RPAREN; break;
        case '{': curr_tok = TOK_LBRACE; break;
        case '}': curr_tok = TOK_RBRACE; break;
        case ';': curr_tok = TOK_SEMICOLON; break;
        case ',': curr_tok = TOK_COMMA; break;
        case '&': if (*src == '&') { src++; curr_tok = TOK_AND; } else curr_tok = TOK_ERR; break;
        case '|': if (*src == '|') { src++; curr_tok = TOK_OR; } else curr_tok = TOK_ERR; break;
        case '=': if (*src == '=') { src++; curr_tok = TOK_EQ; } else curr_tok = TOK_ASSIGN; break;
        case '!': if (*src == '=') { src++; curr_tok = TOK_NE; } else curr_tok = TOK_ERR; break;
        case '<': if (*src == '=') { src++; curr_tok = TOK_LE; } else curr_tok = TOK_LT; break;
        case '>': if (*src == '=') { src++; curr_tok = TOK_GE; } else curr_tok = TOK_GT; break;
        case '+': if (*src == '+') { src++; curr_tok = TOK_INC; } else curr_tok = TOK_PLUS; break;
        case '-': if (*src == '-') { src++; curr_tok = TOK_DEC; } else curr_tok = TOK_MINUS; break;
        case '*': curr_tok = TOK_MUL; break;
        case '/': curr_tok = TOK_DIV; break;
        default: curr_tok = TOK_ERR; break;
    }
}

static int expect(tok_t tok) {
    if (curr_tok == tok) { next_tok(); return 1; }
    error("Unexpected token"); return 0;
}

typedef enum { TYPE_INT, TYPE_STRING, TYPE_DOUBLE, TYPE_BOOL } type_t;
typedef struct { char name[32]; int offset; type_t type; } symbol_t;
static symbol_t symbols[128];
static int symbol_count = 0;

static int find_symbol(const char* name) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbols[i].name, name) == 0) return i;
    }
    return -1;
}

static void add_symbol(const char* name, type_t type) {
    if (symbol_count >= 128) { error("Too many symbols"); return; }
    int i = 0; while (name[i] && i < 31) { symbols[symbol_count].name[i] = name[i]; i++; }
    symbols[symbol_count].name[i] = 0; symbols[symbol_count].offset = symbol_count; symbols[symbol_count].type = type; symbol_count++;
}

static uint8_t* code_buf; static int code_ptr = 0; static uint8_t* string_pool; static int string_ptr = 0;
typedef struct { int code_addr; int string_off; } patch_t;
static patch_t patches[128]; static int patch_count = 0;
static void emit(uint8_t op) { if (code_ptr < 4095) code_buf[code_ptr++] = op; }
static void emit32(int32_t val) { if (code_ptr < 4092) { *(int32_t*)&code_buf[code_ptr] = val; code_ptr += 4; } }

static void parse_logical();
static void parse_block();
static void parse_statement();

static void parse_factor() {
    if (curr_tok == TOK_NUM || curr_tok == TOK_DOUBLE_NUM) { emit(OP_PUSH); emit32(tok_ival); next_tok(); }
    else if (curr_tok == TOK_TRUE || curr_tok == TOK_FALSE) { emit(OP_PUSH); emit32(tok_ival); next_tok(); }
    else if (curr_tok == TOK_ID) {
        int sym_idx = find_symbol(tok_sval);
        if (sym_idx == -1 && strcmp(tok_sval, "endl") != 0) error("Undefined variable");
        else if (sym_idx != -1) emit(OP_LOAD); emit32(symbols[sym_idx].offset);
        next_tok();
    } else if (curr_tok == TOK_LPAREN) { next_tok(); parse_logical(); expect(TOK_RPAREN); }
    else if (curr_tok == TOK_MINUS) {
        next_tok();
        if (curr_tok == TOK_NUM) { emit(OP_PUSH); emit32(-tok_ival); next_tok(); }
        else { parse_factor(); emit(OP_PUSH); emit32(-1); emit(OP_MUL); }
    }
}

static void parse_term() {
    parse_factor();
    while (curr_tok == TOK_MUL || curr_tok == TOK_DIV) {
        tok_t op = curr_tok; next_tok(); parse_factor();
        if (op == TOK_MUL) emit(OP_DMUL); else emit(OP_DDIV);
    }
}

static void parse_expr() {
    parse_term();
    while (curr_tok == TOK_PLUS || curr_tok == TOK_MINUS) {
        tok_t op = curr_tok; next_tok(); parse_term();
        if (op == TOK_PLUS) emit(OP_DADD); else emit(OP_DSUB);
    }
}

static void parse_relational() {
    parse_expr();
    while (curr_tok == TOK_EQ || curr_tok == TOK_NE || curr_tok == TOK_LT || curr_tok == TOK_GT || curr_tok == TOK_LE || curr_tok == TOK_GE) {
        tok_t op = curr_tok; next_tok(); parse_expr();
        if (op == TOK_EQ) emit(OP_EQ); else if (op == TOK_NE) emit(OP_NE); else if (op == TOK_LT) emit(OP_LT); else if (op == TOK_GT) emit(OP_GT);
        else if (op == TOK_LE) { emit(OP_GT); emit(OP_PUSH); emit32(0); emit(OP_EQ); } else if (op == TOK_GE) { emit(OP_LT); emit(OP_PUSH); emit32(0); emit(OP_EQ); } 
    }
}

static void parse_logical() {
    parse_relational();
    while (curr_tok == TOK_AND || curr_tok == TOK_OR) {
        tok_t op = curr_tok; next_tok(); parse_relational();
        if (op == TOK_AND) { emit(OP_PUSH); emit32(0); emit(OP_NE); emit(OP_MUL); emit(OP_PUSH); emit32(0); emit(OP_NE); }
        else { emit(OP_ADD); emit(OP_PUSH); emit32(0); emit(OP_NE); }
    }
}

static void parse_statement() {
    if (curr_tok == TOK_INT || curr_tok == TOK_DOUBLE_TYPE || curr_tok == TOK_BOOL_TYPE || curr_tok == TOK_STRING_TYPE) {
        type_t type = (curr_tok == TOK_INT) ? TYPE_INT : (curr_tok == TOK_DOUBLE_TYPE ? TYPE_DOUBLE : (curr_tok == TOK_BOOL_TYPE ? TYPE_BOOL : TYPE_STRING));
        next_tok();
        if (curr_tok != TOK_ID) { error("Expected identifier"); return; }
        char name[64]; strcpy(name, tok_sval); 
        int sym_idx = find_symbol(name);
        if (sym_idx == -1) { add_symbol(name, type); sym_idx = find_symbol(name); }
        next_tok();
        if (curr_tok == TOK_ASSIGN) {
            next_tok();
            if (type == TYPE_STRING) {
                if (curr_tok == TOK_STRING) {
                    int s_off = string_ptr;
                    if (string_ptr + strlen(tok_sval) + 1 < 1024) { strcpy((char*)&string_pool[string_ptr], tok_sval); string_ptr += strlen(tok_sval) + 1; }
                    emit(OP_PUSH); if (patch_count < 128) { patches[patch_count].code_addr = code_ptr; patches[patch_count].string_off = s_off; patch_count++; } emit32(0);
                    next_tok();
                } else if (curr_tok == TOK_ID) {
                    int src_idx = find_symbol(tok_sval);
                    if (src_idx == -1) error("Undefined variable");
                    else { emit(OP_LOAD); emit32(symbols[src_idx].offset); }
                    next_tok();
                }
            } else if (type == TYPE_DOUBLE) {
                if (curr_tok == TOK_NUM || curr_tok == TOK_DOUBLE_NUM) { emit(OP_PUSH); emit32(tok_ival); next_tok(); }
                else parse_logical();
            } else parse_logical();
            emit(OP_STORE); emit32(symbols[sym_idx].offset);
        }
        if (curr_tok == TOK_SEMICOLON) next_tok();
    } else if (curr_tok == TOK_INPUT) {
        next_tok(); expect(TOK_LPAREN);
        if (curr_tok == TOK_ID) {
            int sym_idx = find_symbol(tok_sval);
            if (sym_idx != -1) {
                emit(OP_PUSH); emit32(symbols[sym_idx].offset);
                if (symbols[sym_idx].type == TYPE_INT) emit(OP_INPUT_INT);
                else if (symbols[sym_idx].type == TYPE_DOUBLE) emit(OP_INPUT_DOUBLE);
                else if (symbols[sym_idx].type == TYPE_STRING) emit(OP_INPUT_STR);
            }
            next_tok();
        }
        expect(TOK_RPAREN);
        if (curr_tok == TOK_SEMICOLON) next_tok();
    } else if (curr_tok == TOK_ID) {
        char name[64]; strcpy(name, tok_sval); next_tok();
        if (curr_tok == TOK_ASSIGN) {
            next_tok(); parse_logical();
            int sym_idx = find_symbol(name);
            if (sym_idx == -1) error("Undefined variable");
            else emit(OP_STORE); emit32(symbols[sym_idx].offset);
            if (curr_tok == TOK_SEMICOLON) next_tok();
        } else if (curr_tok == TOK_INC || curr_tok == TOK_DEC) {
            int sym_idx = find_symbol(name);
            if (sym_idx == -1) error("Undefined variable");
            else { emit(OP_LOAD); emit32(symbols[sym_idx].offset); emit(OP_PUSH); emit32(1); if (curr_tok == TOK_INC) emit(OP_ADD); else emit(OP_SUB); emit(OP_STORE); emit32(symbols[sym_idx].offset); }
            next_tok();
            if (curr_tok == TOK_SEMICOLON) next_tok();
        } else if (curr_tok == TOK_LPAREN) {
            next_tok();
            if (strcmp(name, "print") == 0) {
                while (curr_tok != TOK_RPAREN && curr_tok != TOK_EOF) {
                    if (curr_tok == TOK_STRING) {
                        int s_off = string_ptr;
                        if (string_ptr + strlen(tok_sval) + 1 < 1024) { strcpy((char*)&string_pool[string_ptr], tok_sval); string_ptr += strlen(tok_sval) + 1; }
                        emit(OP_PRINT_STR); 
                        if (patch_count < 128) { patches[patch_count].code_addr = code_ptr; patches[patch_count].string_off = s_off; patch_count++; }
                        emit32(0); next_tok();
                    } else if (curr_tok == TOK_ID && strcmp(tok_sval, "endl") == 0) { emit(OP_PRINT_NL); next_tok(); }
                    else if (curr_tok == TOK_ID) {
                        int sym_idx = find_symbol(tok_sval);
                        if (sym_idx != -1) { emit(OP_LOAD); emit32(symbols[sym_idx].offset);
                            if (symbols[sym_idx].type == TYPE_STRING) emit(OP_PRINT_VAR_STR);
                            else if (symbols[sym_idx].type == TYPE_DOUBLE) emit(OP_PRINT_DOUBLE);
                            else emit(OP_PRINT_INT);
                        } else { parse_logical(); emit(OP_PRINT_INT); }
                        next_tok();
                    } else { parse_logical(); emit(OP_PRINT_INT); }
                    if (curr_tok == TOK_COMMA) next_tok();
                }
            } else if (strcmp(name, "exit") == 0) {
                if (curr_tok != TOK_RPAREN) parse_expr(); else { emit(OP_PUSH); emit32(0); }
                emit(OP_EXIT);
            } else error("Unknown function");
            expect(TOK_RPAREN);
            if (curr_tok == TOK_SEMICOLON) next_tok();
        }
    } else if (curr_tok == TOK_EXIT) {
        next_tok(); expect(TOK_LPAREN);
        if (curr_tok != TOK_RPAREN) parse_expr(); else { emit(OP_PUSH); emit32(0); }
        expect(TOK_RPAREN); emit(OP_EXIT);
        if (curr_tok == TOK_SEMICOLON) next_tok();
    } else if (curr_tok == TOK_SEMICOLON) { next_tok(); }
    else { next_tok(); }
}

static void parse_block() { while (curr_tok != TOK_RBRACE && curr_tok != TOK_EOF) parse_statement(); }

void bcc_main(char* args) {
    char src_file[64], out_file[64];
    int i = 0; while (*args == ' ') args++; while (*args && *args != ' ' && i < 63) src_file[i++] = *args++; src_file[i] = 0;
    if (src_file[0] == 0) { print_string("Usage: bcc <file.bc>\n"); return; }
    strcpy(out_file, src_file);
    int len = strlen(out_file);
    if (len > 3 && strcmp(out_file + len - 3, ".bc") == 0) { strcpy(out_file + len - 3, ".bin"); } else { strcpy(out_file + len, ".bin"); }
    int fd = fs_open(src_file, 0); if (fd < 0) { print_string("Could not open source file\n"); return; }
    src_start = kmalloc(8192); if (!src_start) { fs_close(fd); return; }
    int read_len = fs_read(fd, src_start, 8191); fs_close(fd); src_start[read_len] = 0; src = src_start;
    code_buf = kmalloc(4096); string_pool = kmalloc(1024);
    if (!code_buf || !string_pool) { if (src_start) kfree(src_start); if (code_buf) kfree(code_buf); if (string_pool) kfree(string_pool); return; }
    code_ptr = 0; string_ptr = 0; symbol_count = 0; patch_count = 0; line_num = 1; error_occurred = 0;
    next_tok();
    if (curr_tok == TOK_VOID) {
        next_tok(); expect(TOK_MAIN); expect(TOK_LPAREN); expect(TOK_RPAREN); expect(TOK_LBRACE);
        parse_block(); expect(TOK_RBRACE);
    } else { while (curr_tok != TOK_EOF) parse_statement(); }
    emit(OP_HALT);
    for (int p=0; p<patch_count; p++) { *(int32_t*)&code_buf[patches[p].code_addr] = 4 + code_ptr + patches[p].string_off; }
    int out_fd = fs_create(out_file);
    if (out_fd >= 0) {
        if (!error_occurred) {
            int32_t header = code_ptr;
            fs_write(out_fd, (char*)&header, 4);
            fs_write(out_fd, (char*)code_buf, code_ptr);
            fs_write(out_fd, (char*)string_pool, string_ptr);
            print_string("Compilation successful: "); print_string(out_file); print_string("\n");
        } else { print_string("Compilation failed.\n"); }
        fs_close(out_fd);
    } else { print_string("Could not create output file\n"); }
    kfree(src_start); kfree(code_buf); kfree(string_pool);
}

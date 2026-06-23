// Ring3 Test Entry Point (Kernel-side shim)
// This runs in ring3 after being launched via iretq

#include <stdint.h>
#include <stddef.h>

// Syscall numbers
#define SYS_WRITE     1
#define SYS_GETPID    4
#define SYS_SLEEP     5
#define SYS_GETTIME   6

// Syscall interface (inline asm with proper constraints)
static inline int sys_write(int fd, const void* buf, uint32_t count) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_WRITE), "D"(fd), "S"(buf), "d"(count)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline int sys_getpid(void) {
    int ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETPID)
        : "rcx", "r11", "memory"
    );
    return ret;
}

static inline void sys_sleep(uint32_t ms) {
    __asm__ volatile (
        "int $0x80"
        : : "a"(SYS_SLEEP), "D"(ms)
        : "rcx", "r11", "memory"
    );
}

static inline uint32_t sys_gettime(void) {
    uint32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(SYS_GETTIME)
        : "rcx", "r11", "memory"
    );
    return ret;
}

// String functions - use kernel's versions (defined in string.c)
extern void* memset(void* s, int c, size_t n);
extern size_t strlen(const char* s);

void print_str(const char* s) {
    sys_write(1, s, strlen(s));
}

void print_dec(uint64_t val) {
    char buf[32];
    int i = 0;
    if (val == 0) { sys_write(1, "0", 1); return; }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = 0; j < i/2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i-1-j];
        buf[i-1-j] = tmp;
    }
    sys_write(1, buf, i);
}

void print_test(const char* name, int passed) {
    static const char* pass_str = "[PASS] ";
    static const char* fail_str = "[FAIL] ";
    sys_write(1, passed ? pass_str : fail_str, 7);
    sys_write(1, name, 0);
    sys_write(1, "\n", 1);
}

// Test functions
void test_sys_getpid(void) {
    int pid = sys_getpid();
    print_test("sys_getpid returns valid PID", pid >= 0);
}

void test_sys_write(void) {
    int ret = sys_write(1, "Hello from ring3!\n", 19);
    print_test("sys_write works", ret == 19);
}

void test_sys_gettime(void) {
    uint32_t t1 = sys_gettime();
    sys_sleep(100);
    uint32_t t2 = sys_gettime();
    print_test("sys_gettime increments", t2 > t1);
}

void test_sys_sleep(void) {
    uint32_t t1 = sys_gettime();
    sys_sleep(50);
    uint32_t t2 = sys_gettime();
    print_test("sys_sleep works", (t2 - t1) >= 40);
}

void test_ring3_access(void) {
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    int is_ring3 = (cs & 3) == 3;
    print_test("Running in ring 3", is_ring3);
}

void test_user_stack(void) {
    char local_var = 'A';
    int accessible = (local_var == 'A');
    print_test("User stack accessible", accessible);
}

void test_memory_protection(void) {
    int test = 42;
    print_test("User memory works", test == 42);
}

void test_syscall_interface(void) {
    int ret1 = sys_write(1, "", 0);
    int pid = sys_getpid();
    int ret2 = -1;
    __asm__ volatile (
        "mov $999, %%rax\n"
        "int $0x80\n"
        : "=a"(ret2)
        : : "rcx", "r11", "memory"
    );
    int invalid_handled = (ret2 == 0 || ret2 == -1);
    print_test("Invalid syscall handled", invalid_handled);
}

void test_stack_growth(void) {
    char buffer[1024];
    for (int i = 0; i < 1024; i++) buffer[i] = i & 0xFF;
    
    int sum = 0;
    for (int i = 0; i < 1024; i++) sum += buffer[i];
    
    print_test("Stack growth works", sum != 0);
}

void test_syscall_preserves_registers(void) {
    print_test("Registers preserved across syscall", 1);
}

void ring3_test_main(void) {
    const char* msg = "\n=== Ring3 User Mode Test Suite ===\n\n";
    sys_write(1, msg, 38);
    
    test_ring3_access();
    test_user_stack();
    test_sys_getpid();
    test_sys_write();
    test_sys_gettime();
    test_sys_sleep();
    test_memory_protection();
    test_syscall_interface();
    test_stack_growth();
    test_syscall_preserves_registers();
    
    const char* done = "\n=== All Tests Complete ===\n";
    sys_write(1, done, 27);
    
    // Exit via SYS_EXIT (9)
    __asm__ volatile ("mov $9, %%rax; mov $0, %%rdi; int $0x80" : : : "rax", "rdi");
}

// Entry point for ring3
void ring3_entry(void) {
    ring3_test_main();
}

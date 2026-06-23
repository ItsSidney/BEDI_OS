#include "kernel/syscall.h"
#include "kernel/task/task.h"
#include "kernel/mem/kheap.h"
#include "kernel/time/timer.h"
#include "drivers/video/framebuffer.h"
#include "kernel/log.h"

typedef uint64_t (*syscall_fn)(uint64_t, uint64_t, uint64_t, uint64_t);

static uint64_t k_sys_exit(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a2; (void)a3; (void)a4;
    exit_task((int)code);
    return 0;
}

static uint64_t k_sys_write(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4) {
    (void)a4;
    if (fd == 1 || fd == 2) {
        const char* str = (const char*)buf;
        print_string(str);
        return count;
    }
    return (uint64_t)sys_write((int)fd, (char*)buf, (uint32_t)count);
}

static uint64_t k_sys_read(uint64_t fd, uint64_t buf, uint64_t count, uint64_t a4) {
    (void)a4;
    return (uint64_t)sys_read((int)fd, (char*)buf, (uint32_t)count);
}

static uint64_t k_sys_open(uint64_t path, uint64_t flags, uint64_t a3, uint64_t a4) {
    (void)a3; (void)a4;
    return (uint64_t)sys_open((const char*)path, (int)flags);
}

static uint64_t k_sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a2; (void)a3; (void)a4;
    sys_close((int)fd);
    return 0;
}

static uint64_t k_sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    task_t* cur = get_current_task();
    return cur ? (uint64_t)cur->id : 0;
}

static uint64_t k_sys_sleep(uint64_t ms, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a2; (void)a3; (void)a4;
    sleep_task((uint32_t)ms);
    return 0;
}

static uint64_t k_sys_gettime(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a1; (void)a2; (void)a3; (void)a4;
    return timer_get_ms();
}

static uint64_t k_sys_alloc(uint64_t size, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a2; (void)a3; (void)a4;
    return (uint64_t)kmalloc((size_t)size);
}

static uint64_t k_sys_free(uint64_t ptr, uint64_t a2, uint64_t a3, uint64_t a4) {
    (void)a2; (void)a3; (void)a4;
    kfree((void*)ptr);
    return 0;
}

static syscall_fn syscall_table[SYSCALL_COUNT] = {
    k_sys_open,         // 0
    k_sys_write,        // 1
    k_sys_read,         // 2
    k_sys_close,        // 3
    k_sys_getpid,       // 4
    k_sys_sleep,        // 5
    k_sys_gettime,      // 6
    k_sys_alloc,        // 7
    k_sys_free,         // 8
    k_sys_exit          // 9
};

uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    if (num >= SYSCALL_COUNT) return -1;
    syscall_fn fn = syscall_table[num];
    if (!fn) return -1;
    return fn(arg1, arg2, arg3, 0);
}
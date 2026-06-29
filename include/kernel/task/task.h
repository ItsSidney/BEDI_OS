#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define MAX_TASKS 64
#define MAX_FDS 16
#define KERNEL_STACK_SIZE 16384   // 16 KB kernel stack
#define USER_STACK_SIZE   65536   // 64 KB user stack

typedef enum {
    TASK_FREE,
    TASK_READY,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_DEAD
} task_state_t;

typedef enum {
    TASK_RING0 = 0,    // Kernel thread
    TASK_RING3 = 3     // User process
} task_ring_t;

typedef struct {
    uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rdi, rsi, rdx, rcx, rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) registers_t;

struct vfs_node;

typedef struct {
    int id;
    int ppid;
    task_state_t state;
    task_ring_t ring;           // 0 = kernel, 3 = user
    uint64_t rsp;               // Saved RSP (points to registers_t on stack)
    uint64_t kernel_stack_base; // Kernel stack base
    uint64_t kernel_stack_top;  // Top of kernel stack (current RSP in kernel)
    uint64_t user_stack_base;   // User stack base (for ring3 tasks)
    uint64_t user_stack_top;    // User stack top (initial RSP for user mode)
    uint64_t sleep_until;       // For sleep scheduling
    char name[32];
    
    // Unix-like File Descriptor Table
    struct vfs_node* fds[MAX_FDS];
    uint32_t fd_flags[MAX_FDS];
} task_t;

// Ring 0 (kernel) task creation
int create_task(void (*entry)(void), const char* name);

// Ring 3 (user) process creation
int create_user_process(void (*entry)(void), const char* name);

void init_tasking(void);
void exit_task(int code);
void yield(void);
void sleep_task(uint32_t ms);

// FD operations
int sys_open(const char* path, int flags);
int sys_read(int fd, char* buf, uint32_t count);
int sys_write(int fd, const char* buf, uint32_t count);
void sys_close(int fd);

// ACPI syscalls (from ring3)
int sys_acpi_get_battery_info(void* info);
int sys_acpi_get_battery_status(void* status);

// CPU usage tracking (updated by scheduler on each timer tick)
extern volatile uint64_t cpu_sched_ticks;
extern volatile uint64_t cpu_busy_ticks;

// Called by interrupt handler
uint64_t schedule(uint64_t current_rsp);
task_t* get_current_task(void);
task_t* get_task_by_index(int index);
int get_current_task_id(void);
int get_task_count(void);

void init_tasking(void);

#endif

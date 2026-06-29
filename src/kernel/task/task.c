#include "kernel/task/task.h"
#include "kernel/mem/kheap.h"
#include "kernel/mem/vmm.h"
#include "kernel/time/timer.h"
#include "filesystem/vfs.h"
#include "kernel/arch/x86_64/tss.h"

extern uint64_t hhdm_offset;
extern uint64_t vmm_get_phys(uint64_t virt);

// Forward declarations
extern void syscall_entry_stub();

static uint64_t idle_rsp = 0;
static uint64_t idle_stack[512];

volatile uint64_t cpu_sched_ticks = 0;
volatile uint64_t cpu_busy_ticks = 0;

static void idle_loop(void) {
    while (1) {
        __asm__ volatile("sti; hlt");
    }
}

static task_t tasks[MAX_TASKS];
static int current_task_idx = -1;
static int next_task_id = 1;
static int tasking_enabled = 0;

// Syscall numbers
#define SYS_OPEN        0
#define SYS_READ        1
#define SYS_WRITE       2
#define SYS_CLOSE       3
#define SYS_YIELD       4
#define SYS_SLEEP       5
#define SYS_EXIT        6

task_t* get_current_task(void) {
    if (current_task_idx < 0) return 0;
    return &tasks[current_task_idx];
}

void init_tasking(void) {
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].state = TASK_FREE;
        tasks[i].id = 0;
        for (int f = 0; f < MAX_FDS; f++) tasks[i].fds[f] = 0;
    }
    
    // Create the kernel idle/main task (ring 0)
    tasks[0].id = 0;
    tasks[0].ppid = 0;
    tasks[0].state = TASK_RUNNING;
    tasks[0].ring = TASK_RING0;
    tasks[0].kernel_stack_base = (uint64_t)kmalloc(KERNEL_STACK_SIZE);
    tasks[0].kernel_stack_top = tasks[0].kernel_stack_base + KERNEL_STACK_SIZE;
    tasks[0].user_stack_base = 0;
    tasks[0].user_stack_top = 0;
    tasks[0].name[0] = 'm'; tasks[0].name[1] = 'a'; tasks[0].name[2] = 'i'; tasks[0].name[3] = 'n'; tasks[0].name[4] = '\0';
    
    // Setup idle state
    uint64_t* stack = &idle_stack[512];
    *(--stack) = 0x10;  // SS
    *(--stack) = (uint64_t)&idle_stack[512]; // RSP
    *(--stack) = 0x202; // RFLAGS
    *(--stack) = 0x08;  // CS
    *(--stack) = (uint64_t)idle_loop; // RIP
    for (int i=0; i<15; i++) *(--stack) = 0;
    idle_rsp = (uint64_t)stack;

    current_task_idx = 0;
    tasking_enabled = 1;
}

// Create a kernel thread (ring 0)
int create_task(void (*entry)(void), const char* name) {
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;
    
    tasks[slot].id = next_task_id++;
    tasks[slot].ppid = (current_task_idx >= 0) ? tasks[current_task_idx].id : 0;
    tasks[slot].ring = TASK_RING0;
    tasks[slot].sleep_until = 0;
    for (int f = 0; f < MAX_FDS; f++) tasks[slot].fds[f] = 0;
    
    int j = 0;
    while (name[j] && j < 31) { tasks[slot].name[j] = name[j]; j++; }
    tasks[slot].name[j] = '\0';
    
    // Allocate kernel stack
    tasks[slot].kernel_stack_base = (uint64_t)kmalloc(KERNEL_STACK_SIZE);
    if (!tasks[slot].kernel_stack_base) {
        tasks[slot].state = TASK_FREE;
        return -1;
    }
    
    tasks[slot].kernel_stack_top = tasks[slot].kernel_stack_base + KERNEL_STACK_SIZE;
    tasks[slot].user_stack_base = 0;
    tasks[slot].user_stack_top = 0;
    
    // Setup initial stack frame (kernel mode)
    uint64_t stack_top = tasks[slot].kernel_stack_top;
    uint64_t* stack = (uint64_t*)stack_top;
    
    *(--stack) = 0x10;  // SS = kernel data segment
    *(--stack) = stack_top; // RSP
    *(--stack) = 0x202; // RFLAGS (IF=1)
    *(--stack) = 0x08;  // CS = kernel code segment
    *(--stack) = (uint64_t)entry; // RIP
    
    // 15 general purpose registers (will be popped by POP_ALL)
    for (int i = 0; i < 15; i++) *(--stack) = 0;
    
    tasks[slot].rsp = (uint64_t)stack;
    tasks[slot].state = TASK_READY;
    
    return tasks[slot].id;
}

// Create a user process (ring 3)
int create_user_process(void (*entry)(void), const char* name) {
    int slot = -1;
    for (int i = 1; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_FREE) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;
    
    tasks[slot].id = next_task_id++;
    tasks[slot].ppid = (current_task_idx >= 0) ? tasks[current_task_idx].id : 0;
    tasks[slot].ring = TASK_RING3;
    tasks[slot].sleep_until = 0;
    for (int f = 0; f < MAX_FDS; f++) tasks[slot].fds[f] = 0;
    
    int j = 0;
    while (name[j] && j < 31) { tasks[slot].name[j] = name[j]; j++; }
    tasks[slot].name[j] = '\0';
    
    // Allocate kernel stack
    tasks[slot].kernel_stack_base = (uint64_t)kmalloc(KERNEL_STACK_SIZE);
    if (!tasks[slot].kernel_stack_base) {
        tasks[slot].state = TASK_FREE;
        return -1;
    }
    tasks[slot].kernel_stack_top = tasks[slot].kernel_stack_base + KERNEL_STACK_SIZE;
    
    // Allocate user stack
    tasks[slot].user_stack_base = (uint64_t)kmalloc(USER_STACK_SIZE);
    if (!tasks[slot].user_stack_base) {
        kfree((void*)tasks[slot].kernel_stack_base);
        tasks[slot].state = TASK_FREE;
        return -1;
    }
    tasks[slot].user_stack_top = tasks[slot].user_stack_base + USER_STACK_SIZE;
    
    // Mark user stack pages as user-accessible (U/S bit) so ring 3 can access them.
    // Align to page boundaries to avoid exposing adjacent kernel-only data.
    uint64_t user_stack_page = tasks[slot].user_stack_base & ~(PAGE_SIZE - 1);
    uint64_t user_stack_end = tasks[slot].user_stack_top;
    uint64_t user_stack_size = user_stack_end - user_stack_page;
    vmm_add_flags(user_stack_page, user_stack_size, VMM_USER);
    
    // Build iretq frame on kernel stack for initial entry to user mode
    uint64_t kstack_top = tasks[slot].kernel_stack_top;
    uint64_t* kstack = (uint64_t*)kstack_top;
    
    // Build iretq frame on kernel stack for initial entry to user mode
    *(--kstack) = 0x23;              // SS = user data segment (selector 0x20 + RPL 3)
    *(--kstack) = tasks[slot].user_stack_top; // RSP = top of user stack
    *(--kstack) = 0x202;             // RFLAGS = IF=1
    *(--kstack) = 0x1B;              // CS = user code segment (selector 0x18 + RPL 3)
    *(--kstack) = (uint64_t)entry;   // RIP = entry point
    
    // 15 general purpose registers (initialize to 0)
    for (int i = 0; i < 15; i++) *(--kstack) = 0;
    
    tasks[slot].rsp = (uint64_t)kstack;
    tasks[slot].state = TASK_READY;
    
    return tasks[slot].id;
}

void exit_task(int code) {
    __asm__ volatile("cli");
    if (current_task_idx > 0) {
        tasks[current_task_idx].state = TASK_DEAD;
        // Close all FDs
        for (int i = 0; i < MAX_FDS; i++) {
            if (tasks[current_task_idx].fds[i]) {
                vfs_close(tasks[current_task_idx].fds[i]);
                tasks[current_task_idx].fds[i] = 0;
            }
        }
        kfree((void*)tasks[current_task_idx].kernel_stack_base);
        if (tasks[current_task_idx].user_stack_base) {
            kfree((void*)tasks[current_task_idx].user_stack_base);
        }
    }
    while (1) { __asm__ volatile("sti"); yield(); }
}

// Syscall implementations
int sys_open(const char* path, int flags) {
    struct vfs_node* node = vfs_open_path(path);
    if (!node) return -1;
    
    task_t* cur = get_current_task();
    for (int i = 0; i < MAX_FDS; i++) {
        if (!cur->fds[i]) {
            cur->fds[i] = node;
            return i;
        }
    }
    return -1;
}

int sys_read(int fd, char* buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    task_t* cur = get_current_task();
    if (!cur->fds[fd]) return -1;
    return vfs_read(cur->fds[fd], buf, count, 0);
}

int sys_write(int fd, const char* buf, uint32_t count) {
    if (fd < 0 || fd >= MAX_FDS) return -1;
    task_t* cur = get_current_task();
    if (!cur->fds[fd]) return -1;
    return vfs_write(cur->fds[fd], buf, count, 0);
}

void sys_close(int fd) {
    if (fd < 0 || fd >= MAX_FDS) return;
    task_t* cur = get_current_task();
    if (cur->fds[fd]) {
        vfs_close(cur->fds[fd]);
        cur->fds[fd] = 0;
    }
}

void yield(void) {
    __asm__ volatile("int $0x20");
}

void sleep_task(uint32_t ms) {
    if (tasking_enabled) {
        tasks[current_task_idx].sleep_until = timer_get_ms() + ms;
        tasks[current_task_idx].state = TASK_SLEEPING;
        yield();
    } else {
        sleep_ms(ms);
    }
}

int get_current_task_id(void) {
    task_t* cur = get_current_task();
    return cur ? cur->id : -1;
}

int get_task_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state != TASK_FREE) count++;
    }
    return count;
}

task_t* get_task_by_index(int index) {
    if (index < 0 || index >= MAX_TASKS) return 0;
    if (tasks[index].state == TASK_FREE) return 0;
    return &tasks[index];
}

uint64_t schedule(uint64_t current_rsp) {
    if (!tasking_enabled) return current_rsp;
    
    if (current_task_idx >= 0) {
        task_t* cur = &tasks[current_task_idx];
        cur->rsp = current_rsp;
        if (cur->state == TASK_RUNNING) {
            cur->state = TASK_READY;
        }
    } else {
        idle_rsp = current_rsp;
    }
    
    // Wake up sleeping tasks
    uint64_t current_time = timer_get_ms();
    for (int i = 0; i < MAX_TASKS; i++) {
        if (tasks[i].state == TASK_SLEEPING && current_time >= tasks[i].sleep_until) {
            tasks[i].state = TASK_READY;
        }
    }
    
    // Find next ready task (Round Robin)
    int start_idx = (current_task_idx >= 0) ? current_task_idx : 0;
    int next_task = start_idx;
    int found_ready = 0;
    do {
        next_task = (next_task + 1) % MAX_TASKS;
        if (tasks[next_task].state == TASK_READY) {
            found_ready = 1;
            break;
        }
    } while (next_task != start_idx);
    
    if (!found_ready && current_task_idx >= 0 && tasks[current_task_idx].state == TASK_READY) {
        next_task = current_task_idx;
        found_ready = 1;
    }

    cpu_sched_ticks++;

    if (!found_ready) {
        current_task_idx = -1; // Switch to idle
        tss_set_user_rsp0((uint64_t)&idle_stack[512]);
        return idle_rsp;
    }

    if (next_task != current_task_idx) {
        cpu_busy_ticks++;
    }
    
    current_task_idx = next_task;
    task_t* next = &tasks[current_task_idx];
    next->state = TASK_RUNNING;
    
    // Update TSS with the TOP of next task's kernel stack buffer
    tss_set_user_rsp0(next->kernel_stack_base + KERNEL_STACK_SIZE);
    
    return next->rsp;
}

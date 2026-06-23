#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

// Syscall numbers
#define SYS_OPEN          0
#define SYS_WRITE         1
#define SYS_READ          2
#define SYS_CLOSE         3
#define SYS_GETPID        4
#define SYS_SLEEP         5
#define SYS_GETTIME       6
#define SYS_ALLOC         7
#define SYS_FREE          8
#define SYS_EXIT          9
#define SYSCALL_COUNT    10

// Forward declarations
struct vfs_node;
struct task_t;

int sys_open(const char* path, int flags);
int sys_write(int fd, const char* buf, uint32_t count);
int sys_read(int fd, char* buf, uint32_t count);
void sys_close(int fd);
int get_current_task_id(void);
void sleep_task(uint32_t ms);
uint32_t timer_get_ms(void);
void* kmalloc(size_t size);
void kfree(void* ptr);
void exit_task(int code);

// Syscall handler (called from asm stub)
uint64_t syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3);

#endif
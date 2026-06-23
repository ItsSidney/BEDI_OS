#ifndef _KERNEL_LOG_H
#define _KERNEL_LOG_H

#define LOG_BUF_SIZE 4096

void log_init(void);
void klog(const char* s);
void log_dump(void);

#endif

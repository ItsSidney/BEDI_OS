#ifndef KHEAP_H
#define KHEAP_H

#include <stdint.h>
#include <stddef.h>

// Simple kernel heap allocator
// Uses a static pool of memory for kernel allocations

#define KHEAP_SIZE (1024 * 1024 * 8)  // 8MB kernel heap

// Initialize kernel heap
void kheap_init(void);

// Allocate memory (aligned to 4 bytes)
void* kmalloc(size_t size);

// Allocate aligned memory (for DMA)
void* kmalloc_aligned(size_t size, size_t align);

// Free memory
void kfree(void* ptr);
void kfree_aligned(void* ptr);

// Get free memory
size_t kheap_free(void);

#endif

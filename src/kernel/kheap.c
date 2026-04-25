#include "../../include/kheap.h"
#include "../../include/vga.h"

// Simple bump allocator for kernel heap
// No free() implementation - memory is only allocated, never freed
// Good enough for embedded OS where we just need RX/TX buffers

static uint8_t heap[KHEAP_SIZE];
static size_t heap_used = 0;
static int initialized = 0;

void kheap_init(void) {
    heap_used = 0;
    initialized = 1;
}

void* kmalloc(size_t size) {
    if (!initialized) {
        kheap_init();
    }
    
    // Align to 4 bytes
    size = (size + 3) & ~3;
    
    if (heap_used + size > KHEAP_SIZE) {
        print_string_color("kheap: Out of memory!\n", VGA_COLOR_RED);
        return NULL;
    }
    
    void* ptr = &heap[heap_used];
    heap_used += size;
    
    return ptr;
}

void* kmalloc_aligned(size_t size, size_t align) {
    if (!initialized) {
        kheap_init();
    }
    
    // Align must be power of 2
    if (align & (align - 1)) {
        return NULL;
    }
    
    // Calculate aligned address
    uintptr_t addr = (uintptr_t)&heap[heap_used];
    uintptr_t aligned = (addr + align - 1) & ~(align - 1);
    size_t padding = aligned - addr;
    
    size = (size + 3) & ~3;  // Align size to 4 bytes
    
    if (heap_used + padding + size > KHEAP_SIZE) {
        print_string_color("kheap: Out of memory!\n", VGA_COLOR_RED);
        return NULL;
    }
    
    heap_used += padding;
    void* ptr = &heap[heap_used];
    heap_used += size;
    
    return ptr;
}

void kfree(void* ptr) {
    // Bump allocator - no free implementation
    // In a real OS, we'd implement a proper free list
    (void)ptr;
}

size_t kheap_free(void) {
    return KHEAP_SIZE - heap_used;
}

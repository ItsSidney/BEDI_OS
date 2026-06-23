#include "kernel/mem/kheap.h"

// Better memory management: First-fit linked-list allocator
#define META_SIZE sizeof(struct block_meta)

typedef struct block_meta {
    size_t size;
    int is_free;
    struct block_meta *next;
} block_meta_t;

static uint8_t heap[KHEAP_SIZE];
static block_meta_t* free_list = NULL;
static int initialized = 0;

void kheap_init(void) {
    free_list = (block_meta_t*)heap;
    free_list->size = KHEAP_SIZE - META_SIZE;
    free_list->is_free = 1;
    free_list->next = NULL;
    initialized = 1;
}

static block_meta_t* find_free_block(size_t size) {
    block_meta_t* curr = free_list;
    while (curr) {
        if (curr->is_free && curr->size >= size) return curr;
        curr = curr->next;
    }
    return NULL;
}

static void split_block(block_meta_t* block, size_t size) {
    if (block->size > size + META_SIZE + 32) {
        block_meta_t* new_block = (block_meta_t*)((uint8_t*)block + META_SIZE + size);
        new_block->size = block->size - size - META_SIZE;
        new_block->is_free = 1;
        new_block->next = block->next;
        block->size = size;
        block->next = new_block;
    }
}

void* kmalloc(size_t size) {
    if (!initialized) kheap_init();
    
    // Align to 8 bytes
    size = (size + 7) & ~7;
    
    block_meta_t* block = find_free_block(size);
    if (!block) {
        return NULL;
    }
    
    split_block(block, size);
    block->is_free = 0;
    
    return (void*)((uint8_t*)block + META_SIZE);
}

void* kmalloc_aligned(size_t size, size_t align) {
    if (!initialized) kheap_init();
    
    // Allocate extra space for alignment and to store the original pointer
    size_t total_size = size + align + sizeof(void*);
    void* ptr = kmalloc(total_size);
    if (!ptr) return NULL;
    
    uintptr_t addr = (uintptr_t)ptr + sizeof(void*);
    uintptr_t aligned = (addr + align - 1) & ~(align - 1);
    
    // Store the original pointer right before the aligned address
    ((void**)aligned)[-1] = ptr;
    
    return (void*)aligned;
}

void kfree_aligned(void* ptr) {
    if (!ptr) return;
    // Retrieve original pointer stored before the aligned address
    void* original = ((void**)ptr)[-1];
    kfree(original);
}

static void merge_free_blocks() {
    block_meta_t* curr = free_list;
    while (curr && curr->next) {
        if (curr->is_free && curr->next->is_free) {
            curr->size += META_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_meta_t* block = (block_meta_t*)((uint8_t*)ptr - META_SIZE);
    
    // Basic bounds check
    if ((uint8_t*)block >= heap && (uint8_t*)block < heap + KHEAP_SIZE) {
        block->is_free = 1;
        merge_free_blocks();
    }
}

size_t kheap_free(void) {
    if (!initialized) return KHEAP_SIZE;
    size_t total = 0;
    block_meta_t* curr = free_list;
    while (curr) {
        if (curr->is_free) total += curr->size;
        curr = curr->next;
    }
    return total;
}

#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PAGE_SIZE 4096

// Page table flags
#define VMM_PRESENT    (1ULL << 0)
#define VMM_WRITE      (1ULL << 1)
#define VMM_USER       (1ULL << 2)
#define VMM_PWT        (1ULL << 3)
#define VMM_PCD        (1ULL << 4)
#define VMM_ACCESSED   (1ULL << 5)
#define VMM_DIRTY      (1ULL << 6)
#define VMM_HUGE       (1ULL << 7)
#define VMM_GLOBAL     (1ULL << 8)
#define VMM_NX         (1ULL << 63)

void vmm_init(void);
bool vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);
void vmm_map_range(uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags);
void vmm_add_flags(uint64_t virt, uint64_t size, uint64_t flags);
int vmm_virt_mapped(uint64_t virt);

// Physical Memory Manager
void pmm_init(void);
uint64_t pmm_alloc(void);
void pmm_free(uint64_t phys);
size_t pmm_free_count(void);

#endif

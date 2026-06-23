#include "kernel/mem/vmm.h"
#include "limine.h"
#include "drivers/video/framebuffer.h"

extern uint64_t hhdm_offset;

static uint64_t* current_pml4;

static inline uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

static inline void invlpg(uint64_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

extern void serial_puts(const char* s);

void vmm_init(void) {
    serial_puts("[VMM] Initializing...\n");
    pmm_init();
    current_pml4 = (uint64_t*)(read_cr3() + hhdm_offset);
    serial_puts("[VMM] Initialized\n");
}

static uint64_t* get_next_level(uint64_t* table, uint64_t index, bool allocate) {
    if (table[index] & VMM_PRESENT) {
        return (uint64_t*)((table[index] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    }
    if (!allocate) return NULL;

    uint64_t phys = pmm_alloc();
    if (phys == 0) return NULL;

    uint64_t* virt = (uint64_t*)(phys + hhdm_offset);
    for (int i = 0; i < 512; i++) virt[i] = 0;

    table[index] = phys | VMM_PRESENT | VMM_WRITE | VMM_USER;
    return virt;
}

bool vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_level(current_pml4, pml4_idx, true);
    if (!pdpt) return false;

    uint64_t* pd = get_next_level(pdpt, pdpt_idx, true);
    if (!pd) return false;

    uint64_t* pt = get_next_level(pd, pd_idx, true);
    if (!pt) return false;

    pt[pt_idx] = (phys & 0x000FFFFFFFFFF000ULL) | flags | VMM_PRESENT;
    invlpg(virt);
    return true;
}

void vmm_unmap(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    uint64_t* pdpt = get_next_level(current_pml4, pml4_idx, false);
    if (!pdpt) return;

    uint64_t* pd = get_next_level(pdpt, pdpt_idx, false);
    if (!pd) return;

    uint64_t* pt = get_next_level(pd, pd_idx, false);
    if (!pt) return;

    pt[pt_idx] = 0;
    invlpg(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)(read_cr3() + hhdm_offset);
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx   = (virt >> 21) & 0x1FF;
    uint64_t pt_idx   = (virt >> 12) & 0x1FF;

    if (!(pml4[pml4_idx] & VMM_PRESENT)) return 0;

    uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return 0;

    // 1GB huge page
    if (pdpt[pdpt_idx] & VMM_HUGE) {
        uint64_t base = pdpt[pdpt_idx] & 0x000FFFFFC0000000ULL; // 1GB aligned
        return base | (virt & 0x3FFFFFFF);
    }

    uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pd[pd_idx] & VMM_PRESENT)) return 0;

    // 2MB huge page
    if (pd[pd_idx] & VMM_HUGE) {
        uint64_t base = pd[pd_idx] & 0x000FFFFFFFE00000ULL; // 2MB aligned
        return base | (virt & 0x1FFFFF);
    }

    uint64_t* pt = (uint64_t*)((pd[pd_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pt[pt_idx] & VMM_PRESENT)) return 0;

    return (pt[pt_idx] & 0x000FFFFFFFFFF000ULL) | (virt & 0xFFF);
}

int vmm_virt_mapped(uint64_t virt) {
    uint64_t pml4_idx = (virt >> 39) & 0x1FF;
    uint64_t pdpt_idx = (virt >> 30) & 0x1FF;
    uint64_t pd_idx = (virt >> 21) & 0x1FF;
    uint64_t pt_idx = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = (uint64_t*)(read_cr3() + hhdm_offset);
    if (!(pml4[pml4_idx] & VMM_PRESENT)) return 0;

    uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pdpt[pdpt_idx] & VMM_PRESENT)) return 0;

    // Check for 1GB huge page
    if (pdpt[pdpt_idx] & VMM_HUGE) return 1;

    uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pd[pd_idx] & VMM_PRESENT)) return 0;

    // Check for 2MB huge page
    if (pd[pd_idx] & VMM_HUGE) return 1;

    uint64_t* pt = (uint64_t*)((pd[pd_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
    if (!(pt[pt_idx] & VMM_PRESENT)) return 0;

    return 1;
}

void vmm_add_flags(uint64_t virt, uint64_t size, uint64_t flags) {
    uint64_t hierarchy_flags = flags & (VMM_USER | VMM_WRITE);
    for (uint64_t off = 0; off < size; off += PAGE_SIZE) {
        uint64_t addr = virt + off;
        uint64_t pml4_idx = (addr >> 39) & 0x1FF;
        uint64_t pdpt_idx = (addr >> 30) & 0x1FF;
        uint64_t pd_idx = (addr >> 21) & 0x1FF;
        uint64_t pt_idx = (addr >> 12) & 0x1FF;

        uint64_t* pml4 = current_pml4;
        if (!(pml4[pml4_idx] & VMM_PRESENT)) continue;
        pml4[pml4_idx] |= hierarchy_flags;

        uint64_t* pdpt = (uint64_t*)((pml4[pml4_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        if (!(pdpt[pdpt_idx] & VMM_PRESENT)) continue;
        pdpt[pdpt_idx] |= hierarchy_flags;
        if (pdpt[pdpt_idx] & VMM_HUGE) {
            pdpt[pdpt_idx] |= flags;
            invlpg(addr);
            continue;
        }

        uint64_t* pd = (uint64_t*)((pdpt[pdpt_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        if (!(pd[pd_idx] & VMM_PRESENT)) continue;
        pd[pd_idx] |= hierarchy_flags;
        if (pd[pd_idx] & VMM_HUGE) {
            pd[pd_idx] |= flags;
            invlpg(addr);
            continue;
        }

        uint64_t* pt = (uint64_t*)((pd[pd_idx] & 0x000FFFFFFFFFF000ULL) + hhdm_offset);
        if (!(pt[pt_idx] & VMM_PRESENT)) continue;

        pt[pt_idx] |= flags;
        invlpg(addr);
    }
}

static inline void flush_tlb(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

void vmm_map_range(uint64_t virt, uint64_t phys, uint64_t size, uint64_t flags) {
    for (uint64_t i = 0; i < size; i += PAGE_SIZE) {
        uint64_t pml4_idx = ((virt + i) >> 39) & 0x1FF;
        uint64_t pdpt_idx = ((virt + i) >> 30) & 0x1FF;
        uint64_t pd_idx = ((virt + i) >> 21) & 0x1FF;
        uint64_t pt_idx = ((virt + i) >> 12) & 0x1FF;

        uint64_t* pdpt = get_next_level(current_pml4, pml4_idx, true);
        if (!pdpt) return;
        uint64_t* pd = get_next_level(pdpt, pdpt_idx, true);
        if (!pd) return;
        uint64_t* pt = get_next_level(pd, pd_idx, true);
        if (!pt) return;

        pt[pt_idx] = ((phys + i) & 0x000FFFFFFFFFF000ULL) | flags | VMM_PRESENT;
    }
    flush_tlb();
}

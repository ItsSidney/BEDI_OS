#include "../../include/tss.h"

tss_t tss;

void init_tss() {
    // Clear TSS
    for (unsigned int i = 0; i < sizeof(tss_t); i++) {
        ((char*)&tss)[i] = 0;
    }
    
    // Set up Ring 0 stack (kernel stack)
    tss.rsp0 = 0x90000;  // Kernel stack pointer
    
    // Set I/O map base to end of TSS (no I/O permission map)
    tss.iomap_base = sizeof(tss_t);
}

void set_tss_stack(uint64_t stack0) {
    tss.rsp0 = stack0;
}

#include "../../include/idt.h"
#include "../../include/keyboard.h"

idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t idt_ptr;

extern void irq1_handler();

void set_idt_entry(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

static void remap_pic() {
    // ICW1
    port_byte_out(0x20, 0x11);
    port_byte_out(0xA0, 0x11);
    // ICW2 (Remap to 0x20-0x2F)
    port_byte_out(0x21, 0x20); 
    port_byte_out(0xA1, 0x28); 
    // ICW3
    port_byte_out(0x21, 0x04);
    port_byte_out(0xA1, 0x02);
    // ICW4
    port_byte_out(0x21, 0x01);
    port_byte_out(0xA1, 0x01);
    
    // MASK IRQs
    // 0xFD = 11111101 (Only IRQ 1 enabled - Keyboard)
    port_byte_out(0x21, 0xFD);
    // 0xFF = 11111111 (All IRQs on slave PIC disabled)
    port_byte_out(0xA1, 0xFF);
}

void init_idt() {
    idt_ptr.limit = IDT_SIZE - 1;
    idt_ptr.base = (uint64_t)&idt;
    for (int i = 0; i < IDT_ENTRIES; i++) set_idt_entry(i, 0, 0, 0);
    
    // keyboard
    set_idt_entry(0x21, (uint64_t)irq1_handler, 0x28, 0x8E); 
    
    remap_pic();
    
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
    __asm__ volatile("sti");
}

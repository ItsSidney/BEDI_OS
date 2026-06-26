#include "kernel/arch/x86_64/idt.h"
#include "drivers/input/keyboard.h"
#include "drivers/video/framebuffer.h"

idt_entry_t idt[IDT_ENTRIES];
idt_ptr_t idt_ptr;

extern void irq0_handler();
extern void irq1_handler();
extern void irq12_handler();
extern void syscall_handler_stub();

// ISRs from interrupts.asm
extern void isr0(); extern void isr1(); extern void isr2(); extern void isr3();
extern void isr4(); extern void isr5(); extern void isr6(); extern void isr7();
extern void isr8(); extern void isr9(); extern void isr10(); extern void isr11();
extern void isr12(); extern void isr13(); extern void isr14(); extern void isr15();
extern void isr16(); extern void isr17(); extern void isr18(); extern void isr19();
extern void isr20(); extern void isr21(); extern void isr22(); extern void isr23();
extern void isr24(); extern void isr25(); extern void isr26(); extern void isr27();
extern void isr28(); extern void isr29(); extern void isr30(); extern void isr31();

//extern void serial_puts(const char* s);
extern void itoa(uint64_t n, char* s);

void set_idt_entry(int num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_low = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].type_attr = flags;
    idt[num].offset_mid = (base >> 16) & 0xFFFF;
    idt[num].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].zero = 0;
}

void core_exception_handler(uint64_t* registers) {
    uint64_t isr_num = registers[15];
    uint64_t err_code = registers[16];
    
//    serial_puts("\n[BEDI] CPU EXCEPTION: ");
    char buf[16]; itoa(isr_num, buf);
    print_string("\n  !!! CPU CRITICAL FAULT: ");
    print_string(buf);
    print_string(" !!!\n");

    if (isr_num == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        print_string("  Page Fault at: 0x");
        char hex[20];
        // Simple hex conversion
        const char* hex_chars = "0123456789ABCDEF";
        for (int i = 0; i < 16; i++) {
            hex[15-i] = hex_chars[(cr2 >> (i * 4)) & 0x0F];
        }
        hex[16] = 0;
        print_string(hex);
        print_string("\n  Error Code: ");
        itoa(err_code, buf);
        print_string(buf);
        print_string("\n");
    }

    print_string("  System Halted to prevent data corruption.");
    swap_buffers();
    
    while(1) { __asm__ ("cli; hlt"); }
}

// I/O delay for 8259A PIC: required on real hardware between ICW bytes.
// Port 0x80 is the POST diagnostic port — writing to it is a safe ~1us delay.
static inline void pic_io_wait(void) {
    port_byte_out(0x80, 0);
}

static void remap_pic() {
    // Save current masks
    uint8_t mask1 = port_byte_in(0x21);
    uint8_t mask2 = port_byte_in(0xA1);

    // ICW1: Start initialization sequence, cascade mode, ICW4 needed
    port_byte_out(0x20, 0x11); pic_io_wait();
    port_byte_out(0xA0, 0x11); pic_io_wait();

    // ICW2: Remap IRQ base vectors (master 0x20, slave 0x28)
    port_byte_out(0x21, 0x20); pic_io_wait();
    port_byte_out(0xA1, 0x28); pic_io_wait();

    // ICW3: Tell master that slave is at IRQ2, tell slave its cascade identity
    port_byte_out(0x21, 0x04); pic_io_wait();  // Master: slave on IRQ2
    port_byte_out(0xA1, 0x02); pic_io_wait();  // Slave: cascade identity = 2

    // ICW4: 8086 mode
    port_byte_out(0x21, 0x01); pic_io_wait();
    port_byte_out(0xA1, 0x01); pic_io_wait();

    // OCW1: Set interrupt masks
    // Master: 0xF8 = 11111000 — enable IRQ0 (timer), IRQ1 (kbd), IRQ2 (cascade)
    // Slave:  0xEF = 11101111 — enable IRQ12 (mouse, bit 4 of slave)
    port_byte_out(0x21, 0xF8); pic_io_wait();
    port_byte_out(0xA1, 0xEF); pic_io_wait();

    (void)mask1; (void)mask2; // masks saved but not restored — we set new ones above
}

void init_idt() {
    idt_ptr.limit = IDT_SIZE - 1;
    idt_ptr.base = (uint64_t)&idt;
    for (int i = 0; i < IDT_ENTRIES; i++) set_idt_entry(i, 0, 0, 0);
    
    // Register Exceptions (0-31)
    void (*isrs[])() = {
        isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7,
        isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
    };
    for (int i = 0; i < 32; i++) {
        set_idt_entry(i, (uint64_t)isrs[i], 0x08, 0x8E);
    }
    
    // IRQ0 — timer
    set_idt_entry(0x20, (uint64_t)irq0_handler, 0x08, 0x8E);
    // IRQ1 — keyboard
    set_idt_entry(0x21, (uint64_t)irq1_handler, 0x08, 0x8E); 
    // IRQ12 — mouse
    set_idt_entry(0x2C, (uint64_t)irq12_handler, 0x08, 0x8E);
    
    // Syscall interrupt (int 0x80) — Ring 3 accessible
    set_idt_entry(0x80, (uint64_t)syscall_handler_stub, 0x08, 0xEE);

    remap_pic();
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

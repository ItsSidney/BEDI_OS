[bits 64]

extern keyboard_handler

global irq1_handler

%macro irq_stub 2
%1:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    ; Align stack to 16 bytes
    sub rsp, 8
    
    call %2
    
    add rsp, 8
    
    ; Send EOI to Master PIC
    mov al, 0x20
    out 0x20, al
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq
%endmacro

irq_stub irq1_handler, keyboard_handler

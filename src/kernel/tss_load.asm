; TSS loading and setup
global load_tss
extern tss

load_tss:
    ; Load TSS selector into task register
    mov ax, 0x2B  ; TSS selector (index 5, TI=0, RPL=0)
    ltr ax
    ret

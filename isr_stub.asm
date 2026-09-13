[bits 32]
global irq1_stub
extern process_keyboard_scancode

align 4
irq1_stub:
    pusha
    push ds
    push es
    push fs
    push gs

    ; Segment Switch
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Read Scan Code from Port 0x60
    in al, 0x60
    movzx eax, al
    push eax
    call process_keyboard_scancode
    add esp, 4

    ; Send PIC EOI Signal
    mov al, 0x20
    out 0x20, al

    pop gs
    pop fs
    pop es
    pop ds
    popa
    iretd
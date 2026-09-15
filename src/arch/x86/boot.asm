[bits 32]

; --- Multiboot Header ---
section .multiboot
    align 4
    dd 0x1BADB002             ; MAGIC
    dd 0x03                   ; FLAGS: ALIGN | MEMINFO (lower memory + memory map)
    dd -(0x1BADB002 + 0x03)   ; CHECKSUM

section .bss
    align 16
global stack_top
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
global multiboot_magic
global multiboot_info
extern kernel_main

multiboot_magic: dd 0
multiboot_info:  dd 0

_start:
    cli                       ; interrupts off until kernel sets up GDT/IDT
    mov esp, stack_top

    ; เก็บ multiboot magic + info pointer ไว้ใน global ให้ kernel อ่าน
    mov [multiboot_magic], eax
    mov [multiboot_info], ebx

    call kernel_main

.hang:
    cli
    hlt
    jmp .hang

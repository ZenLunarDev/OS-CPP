.section .text

/* -------------------------------------------------------------
 * GDT Flush: สลับ Code/Data Segments ไปยัง Kernel Selectors
 * ------------------------------------------------------------- */
.global gdt_flush_asm
gdt_flush_asm:
    mov 4(%esp), %eax
    lgdt (%eax)

    mov $0x10, %ax      /* 0x10 = Kernel Data Segment Selector */
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    jmp $0x08, $flush_cs /* 0x08 = Kernel Code Segment Selector */
flush_cs:
    ret

/* -------------------------------------------------------------
 * IDT Load: โหลด IDTR Pointer
 * ------------------------------------------------------------- */
.global idt_load_asm
idt_load_asm:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

/* -------------------------------------------------------------
 * Default Exception Handler
 * ------------------------------------------------------------- */
.global default_isr_handler
default_isr_handler:
    cli
1:  hlt
    jmp 1b

/* -------------------------------------------------------------
 * IRQ1 Handler (Keyboard)
 * ------------------------------------------------------------- */
.global irq1_stub
.extern irq1_handler
irq1_stub:
    pusha
    call irq1_handler
    popa
    iret
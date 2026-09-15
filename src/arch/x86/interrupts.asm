[bits 32]
section .text

global gdt_flush
global idt_load_asm
global tss_flush
global irq0_stub
global irq1_stub
global syscall_stub
global isr_stub_table
global isr_err_table
extern irq0_c_handler
extern irq1_c_handler
extern isr_dispatch
extern syscall_handler

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush_cs
.flush_cs:
    ret

idt_load_asm:
    mov eax, [esp+4]
    lidt [eax]
    ret

tss_flush:
    mov ax, 0x2B
    ltr ax
    ret

irq0_stub:
    pusha
    call irq0_c_handler          ; callee รับ regs* = esp เดิม แต่ไม่ใช้
    popa
    iret

irq1_stub:
    pusha
    in al, 0x60                  ; อ่าน scancode จาก keyboard controller
    movzx eax, al
    push eax
    call irq1_c_handler
    add esp, 4
    mov al, 0x20                 ; EOI ไป PIC master
    out 0x20, al
    popa
    iret

; ---- CPU Exception stubs ----
; Frame ที่ส่งให้ isr_dispatch ต้องตรงกับ struct Registers ใน isr.h ทุก field:
;   ds, edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax, int_no, err_code,
;   eip, cs, eflags
; pusha = edi, esi, ebp, esp, ebx, edx, ecx, eax (เรา push ds แทน esp เดิม)

; Frame ต้องตรงกับ struct Registers ใน isr.h เป๊ะ — memory จากต่ำไปสูง:
;   ds, edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax, int_no, err_code,
;   eip, cs, eflags
; push กลับกันจากท้าย struct ขึ้นบน: (NOERR: err=0 แล้ว int_no) → pusha → ds
; (ERR: CPU err_code อยู่ใต้ eip อยู่แล้ว — push int_no ก่อน pusha เพื่อให้
;  err_code จาก CPU กลายเป็น slot err_code พอดี)
%macro ISR_NOERR 1
isr%1:
    push 0           ; err_code = 0
    push %1          ; int_no
    pusha
    push ds
    mov eax, esp
    push eax         ; Registers*
    call isr_dispatch
    add esp, 4       ; Registers*
    pop ds
    popa
    add esp, 8       ; int_no + err_code
    iretd
%endmacro

%macro ISR_ERR 1
isr%1:
    push %1          ; int_no (push ก่อน pusha!)
    pusha
    push ds
    mov eax, esp
    push eax         ; Registers*
    call isr_dispatch
    add esp, 4       ; Registers*
    pop ds
    popa
    add esp, 8       ; int_no + CPU err_code
    iretd
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_ERR   18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ---- ตาราง address ของ stub ทุกตัว ให้ C ฝั่งอ่านไปตั้ง IDT ได้ ----
isr_stub_table:
%assign i 0
%rep 32
    dd isr%+i
%assign i i+1      ; ★ ต้องเป็น 'i i+1' — '%assign i+1' แปลว่า i=+1 ทุกรอบ (ทุก gate ชี้ isr1!)
%endrep

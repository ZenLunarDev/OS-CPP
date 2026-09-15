[bits 32]

; ---- hello: โปรแกรม Ring 3 ตัวแรกของ MeowOS ----
; Link ที่ VMA 0x40000000 ด้วย user.ld แล้วแปลงเป็น flat binary
; Kernel จะโหลด binary นี้ลง USER_BASE แล้ว jump เข้า _start ใน Ring 3

global _start

section .rodata
msg: db 10, "[hello] Greetings from a REAL user program!", 10, 0

section .text
_start:
    mov eax, 1              ; SYS_WRITE
    mov ebx, msg            ; pointer อยู่ใน user space (0x40000xxx) — ผ่านการ validate
    int 0x80

    mov eax, 2              ; SYS_EXIT
    int 0x80

.hang:
    jmp .hang

[bits 32]
org 0x40000000

; ---- usercrash: user program ที่พยายามเขียน kernel memory ----
; สร้างเป็น flat binary (nasm -f bin) โหลดที่ USER_BASE พอดี
; ใช้พิสูจน์ว่า identity map ของ kernel เป็น supervisor-only จริง (#1)
; ทุก address reference เป็น absolute ที่ VMA จริง (org) จึงถูกหลัง copy

; SYS_WRITE ก่อน (โชว์ว่า syscall ปกติยังทำงาน)
    mov eax, 1
    mov ebx, kernel_write_msg
    int 0x80

; พยายามเขียน kernel image ที่ 0x00100000 (supervisor-only page)
    mov eax, 0x00100000
    mov dword [eax], 0xDEADBEEF     ; <-- ต้อง page fault ทันทีใน Ring 3

; ไม่ควรมาถึงตรงนี้ — ถ้ามาแปลว่า protection พัง
    mov eax, 2
    int 0x80

.hang:
    jmp .hang

kernel_write_msg: db 10, "[usercrash] Now writing to kernel memory (should fault)...", 10, 0

[bits 32]
global enter_user_mode
global user_mode_return

extern user_mode_loop

section .text

; void enter_user_mode(uint32_t entry_point, uint32_t user_stack);
enter_user_mode:
    push ebp
    mov ebp, esp
    cli                 ; ปิด interrupt ระหว่างสลับ mode

    mov ecx, [ebp + 8]  ; Entry Point (EIP)
    mov edx, [ebp + 12] ; User Stack Pointer (ESP)

    mov ax, 0x23        ; User Data Segment Selector (Index 4 | RPL 3 = 0x23)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push 0x23           ; SS (User Stack Segment)
    push edx            ; ESP (User Stack)

    pushfd              ; เอา EFLAGS ปัจจุบันมาแก้ (คงค่า IF/IOPL เดิม)
    pop eax
    or eax, 0x200       ; บังคับ IF = 1 เพื่อให้ user mode รับ interrupt/timer ได้
    push eax

    push 0x1B           ; CS (User Code Segment: 0x18 | RPL 3)
    push ecx            ; EIP (Entry Point)

    iretd               ; CPU pop ค่าทั้งหมด → สลับเป็น Ring 3

; ---- ทางกลับจาก user context (SYS_EXIT / exception kill) ----
; void user_mode_return(uint32_t new_stack) — noreturn
; เรียกจาก C บน kernel stack ของ syscall/fault handler: สลับไป stack ใหม่,
; โหลด segment registers กลับเป็น kernel (ป้องกัน ds=0x23 ค้างจาก Ring 3)
; แล้ว jump เข้า shell loop ตรงๆ — ไม่พึ่ง iretd frame ที่ fault path ทำให้เป็น garbage
user_mode_return:
    mov esp, [esp+4]        ; arg1 = ตำแหน่ง stack ใหม่ (บน kernel stack ของ shell)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    sti                     ; เปิด interrupt กลับ — shell ต้องรับ keyboard/timer
    jmp user_mode_loop
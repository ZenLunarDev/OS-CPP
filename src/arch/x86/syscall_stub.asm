[bits 32]
global syscall_stub
extern syscall_handler

; เข้ามา: [esp] = EIP, [esp+4] = CS, [esp+8] = EFLAGS (+SS/ESP ถ้ามาจาก Ring 3)
; pusha เก็บจาก low→high: edi@+0 esi@+4 ebp@+8 esp@+12 ebx@+16 edx@+20 ecx@+24 eax@+28
;                        (eax อยู่บนสุดของ block!) → EIP@+32, CS@+36
; popa คืนค่าจาก stack เสมอ → eax/ecx/edx ใช้เป็น scratch ได้หลัง pusha
syscall_stub:
    pusha
    mov eax, [esp + 36]     ; CS ของผู้เรียก (EIP อยู่ +32)
    and eax, 3              ; CPL (3 = user, 0 = kernel)
    push eax                ; arg5: caller_ring
    push edx                ; arg4: arg3
    push ecx                ; arg3: arg2
    push ebx                ; arg2: arg1
    push dword [esp + 44]   ; arg1: syscall num (saved eax = pusha top +28; +16 จาก push 4 ตัว)
    call syscall_handler
    add esp, 20

    popa
    iretd
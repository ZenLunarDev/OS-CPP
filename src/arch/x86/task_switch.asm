[bits 32]
global switch_to_task

; void switch_to_task(uint32_t* old_esp, uint32_t new_esp);
switch_to_task:
    ; 1. บันทึก Registers ของ Task ปัจจุบันลง Stack
    pushfd
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi

    ; 2. บันทึก ESP ปัจจุบันลง pointer (*old_esp)
    mov eax, [esp + 36]     ; param 1: old_esp
    mov [eax], esp

    ; 3. โหลด ESP ใหม่ (new_esp)
    mov esp, [esp + 40]     ; param 2: new_esp

    ; 4. Restore Registers ของ Task ใหม่
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    popfd

    ret
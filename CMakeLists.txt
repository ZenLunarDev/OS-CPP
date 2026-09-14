cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)

# 1. ระบุ Full Path ของ Toolchain ในเครื่อง (เปลี่ยน Path ด้านล่างให้ตรงกับตำแหน่งจริง)
set(TOOLCHAIN_PATH "C:/i686-elf-tools-windows/bin") # <--- แก้จุดนี้
set(NASM_PATH "C:/Users/Lunar/AppData/Local/bin/NASM")      # <--- แก้จุดนี้ (ถ้าลงผ่าน Installer ปกติ)

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/i686-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/i686-elf-g++.exe")
set(CMAKE_ASM_NASM_COMPILER "${NASM_PATH}/nasm.exe")

# บังคับไม่ให้ CMake รันการทดสอบการลิงก์ (Prevent Try-Compile Failures ใน Bare-Metal)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(MeowOS C CXX ASM_NASM)

set(KERNEL_CXX_FLAGS "-m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions -fno-rtti")
set(KERNEL_ASM_FLAGS "-f elf32")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KERNEL_CXX_FLAGS}")
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} ${KERNEL_ASM_FLAGS}")

set(SOURCES
        isr_exception_stubs.asm
        interrupts.cpp
        isr.cpp
        timer.cpp
        rtc.cpp
        shell_engine.cpp
        vga_driver.cpp
        pmm.cpp
        paging.cpp
        heap.cpp
        kernel_main.cpp
)

add_executable(kernel.bin ${SOURCES})

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set_target_properties(kernel.bin PROPERTIES
        LINK_FLAGS "-m32 -nostdlib -Wl,-T,${LINKER_SCRIPT}"
        SUFFIX ""
)
cmake_minimum_required(VERSION 3.20)

set(TOOLCHAIN_PATH "C:/i686-elf-tools-windows/bin")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/i686-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/i686-elf-g++.exe")
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PATH}/i686-elf-as.exe")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i386)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(MeowOS C CXX ASM)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffreestanding -fno-builtin -fno-exceptions -fno-rtti -nostdlib -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffreestanding -fno-builtin -nostdlib -Wall -Wextra")
set(CMAKE_ASM_FLAGS "")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker.ld)

# รวม Source ทั้งหมด
add_executable(MeowOS.bin
        boot.s
        interrupts.s
        kernel.cpp
        gdt.cpp
        idt.cpp
        keyboard.cpp
)

# บังคับ Linker Flag และ ลำดับ Placement
set_target_properties(MeowOS.bin PROPERTIES
        SUFFIX ""
        OUTPUT_NAME "MeowOS.bin"
        LINK_FLAGS "-T ${LINKER_SCRIPT} -nostdlib -nodefaultlibs -Wl,--no-warn-rwx-segments"
)
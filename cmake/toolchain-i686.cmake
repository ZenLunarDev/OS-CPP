# MeowOS toolchain: i686 cross compiler โดยมี host gcc -m32 เป็น fallback
#
# ลำดับการหา compiler:
#   1. i686-elf-gcc จาก -DMEOWOS_TOOLCHAIN=<bin-dir> หรือ $MEOWOS_TOOLCHAIN หรือ PATH
#   2. fallback: host gcc/g++ ที่รองรับ -m32 (Linux + gcc-multilib, ใช้ใน CI)
#
# NASM หาจาก -DMEOWOS_NASM=<bin-dir>, $MEOWOS_NASM หรือ PATH

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ---- หา i686-elf toolchain ----
set(MEOWOS_TOOLCHAIN "" CACHE PATH "Directory containing i686-elf-gcc (optional)")
if(MEOWOS_TOOLCHAIN)
    list(APPEND CMAKE_PROGRAM_PATH "${MEOWOS_TOOLCHAIN}")
endif()
if(DEFINED ENV{MEOWOS_TOOLCHAIN} AND NOT "$ENV{MEOWOS_TOOLCHAIN}" STREQUAL "")
    list(APPEND CMAKE_PROGRAM_PATH "$ENV{MEOWOS_TOOLCHAIN}")
endif()

# Probe ตำแหน่งติดตั้งที่พบบ่อย (ใครติดตั้งไว้ที่อื่น ใช้ -DMEOWOS_TOOLCHAIN=<dir> แทน)
foreach(_loc "C:/i686-elf-tools-windows/bin" "$ENV{HOME}/opt/cross/bin" "/usr/local/i686-elf/bin")
    if(EXISTS "${_loc}/i686-elf-gcc.exe" OR EXISTS "${_loc}/i686-elf-gcc")
        list(APPEND CMAKE_PROGRAM_PATH "${_loc}")
    endif()
endforeach()

find_program(MEOWOS_CROSS_C   i686-elf-gcc)
find_program(MEOWOS_CROSS_CXX i686-elf-g++)

if(MEOWOS_CROSS_C AND MEOWOS_CROSS_CXX)
    set(CMAKE_C_COMPILER   "${MEOWOS_CROSS_C}")
    set(CMAKE_CXX_COMPILER "${MEOWOS_CROSS_CXX}")
    set(MEOWOS_USING_CROSS TRUE)
else()
    # ---- Fallback: host gcc พร้อม -m32 (ต้องติดตั้ง gcc-multilib) ----
    find_program(MEOWOS_HOST_C   gcc)
    find_program(MEOWOS_HOST_CXX g++)
    if(NOT MEOWOS_HOST_C OR NOT MEOWOS_HOST_CXX)
        message(FATAL_ERROR
            "ไม่พบ compiler: ต้องการ i686-elf-gcc หรือ host gcc พร้อม -m32\n"
            "ระบุ path ผ่าน -DMEOWOS_TOOLCHAIN=<dir> หรือติดตั้ง gcc-multilib")
    endif()
    set(CMAKE_C_COMPILER   "${MEOWOS_HOST_C}")
    set(CMAKE_CXX_COMPILER "${MEOWOS_HOST_CXX}")
    set(MEOWOS_USING_CROSS FALSE)
    message(STATUS "MeowOS: ใช้ host gcc -m32 (ไม่พบ i686-elf toolchain)")
endif()

# ---- NASM ----
set(MEOWOS_NASM "" CACHE PATH "Directory containing nasm (optional)")
if(MEOWOS_NASM)
    list(APPEND CMAKE_PROGRAM_PATH "${MEOWOS_NASM}")
endif()
if(DEFINED ENV{MEOWOS_NASM} AND NOT "$ENV{MEOWOS_NASM}" STREQUAL "")
    list(APPEND CMAKE_PROGRAM_PATH "$ENV{MEOWOS_NASM}")
endif()

# Probe ตำแหน่งติดตั้ง NASM ที่พบบ่อย (Windows: ใต้ LocalAppData, *nix: PATH ปกติครอบอยู่แล้ว)
if(DEFINED ENV{LOCALAPPDATA})
    foreach(_nloc "$ENV{LOCALAPPDATA}/bin/NASM" "$ENV{LOCALAPPDATA}/NASM" "$ENV{LOCALAPPDATA}/Programs/NASM")
        if(EXISTS "${_nloc}/nasm.exe")
            list(APPEND CMAKE_PROGRAM_PATH "${_nloc}")
        endif()
    endforeach()
endif()

find_program(MEOWOS_NASM_EXE nasm)
if(NOT MEOWOS_NASM_EXE)
    message(FATAL_ERROR "ไม่พบ nasm — ติดตั้ง หรือระบุ path ผ่าน -DMEOWOS_NASM=<dir>")
endif()

set(CMAKE_ASM_NASM_COMPILER "${MEOWOS_NASM_EXE}")

# ---- User program linking (ld + objcopy จาก toolchain เดียวกัน) ----
if(MEOWOS_USING_CROSS)
    find_program(MEOWOS_LD   i686-elf-ld)
    find_program(MEOWOS_OBJCOPY i686-elf-objcopy)
else()
    # ถ้าไม่มี cross binutils ให้ลอง i686-elf ก่อน แล้วค่อย host เวอร์ชัน 32-bit
    find_program(MEOWOS_LD   NAMES i686-elf-ld ld)
    find_program(MEOWOS_OBJCOPY NAMES i686-elf-objcopy objcopy)
endif()

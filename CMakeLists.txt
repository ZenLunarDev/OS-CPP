cmake_minimum_required(VERSION 3.20)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(TOOLCHAIN_PATH "C:/i686-elf-tools-windows/bin")
set(NASM_PATH "C:/Users/Lunar/AppData/Local/bin/NASM")

set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/i686-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/i686-elf-g++.exe")
set(CMAKE_ASM_NASM_COMPILER "${NASM_PATH}/nasm.exe")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(MeowOS C CXX ASM_NASM)

set(COMMON_FLAGS "-m32 -ffreestanding -O2 -Wall -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_NASM_FLAGS "${CMAKE_ASM_NASM_FLAGS} -f elf32")

include_directories(
        include/kernel
        include/drivers
        include/mem
        include/fs
        include/user
)

file(GLOB_RECURSE SOURCES
        "src/*.cpp"
        "src/*.c"
        "src/*.asm"
        "src/*.s"
)

add_executable(kernel.bin ${SOURCES})

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set_target_properties(kernel.bin PROPERTIES
        LINK_FLAGS "-m32 -nostdlib -Wl,-T,${LINKER_SCRIPT}"
        SUFFIX ""
)

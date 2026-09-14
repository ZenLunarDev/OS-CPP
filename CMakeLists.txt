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

# ---- User Programs (flat binary โหลดลง USER_BASE ตอน runtime) ----
set(USER_LD_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/user/user.ld")
set(USER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# hello: nasm → link ที่ 0x40000000 → flat binary → ELF object สำหรับ embed
add_custom_command(
        OUTPUT ${USER_OUT_DIR}/hello.o_embed
        COMMAND ${NASM_PATH}/nasm.exe -f elf32 ${CMAKE_CURRENT_SOURCE_DIR}/user/user_hello.asm -o hello_user.o
        COMMAND ${TOOLCHAIN_PATH}/i686-elf-ld.exe -T ${USER_LD_SCRIPT} -o hello.elf hello_user.o
        COMMAND ${TOOLCHAIN_PATH}/i686-elf-objcopy.exe -O binary hello.elf hello.bin
        COMMAND ${TOOLCHAIN_PATH}/i686-elf-objcopy.exe -I binary -O elf32-i386 -B i386 hello.bin hello.o_embed
        DEPENDS user/user_hello.asm user/user.ld
        WORKING_DIRECTORY ${USER_OUT_DIR}
        COMMENT "Building user program: hello"
)

# usercrash: nasm -f bin ตรงๆ → ELF object สำหรับ embed
add_custom_command(
        OUTPUT ${USER_OUT_DIR}/usercrash.o_embed
        COMMAND ${NASM_PATH}/nasm.exe -f bin ${CMAKE_CURRENT_SOURCE_DIR}/user/usercrash.asm -o usercrash.bin
        COMMAND ${TOOLCHAIN_PATH}/i686-elf-objcopy.exe -I binary -O elf32-i386 -B i386 usercrash.bin usercrash.o_embed
        DEPENDS user/usercrash.asm
        WORKING_DIRECTORY ${USER_OUT_DIR}
        COMMENT "Building user program: usercrash"
)

add_custom_target(user_programs DEPENDS ${USER_OUT_DIR}/hello.o_embed ${USER_OUT_DIR}/usercrash.o_embed)

add_executable(kernel.bin ${SOURCES})
add_dependencies(kernel.bin user_programs)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set_target_properties(kernel.bin PROPERTIES
        LINK_FLAGS "-m32 -nostdlib -Wl,-T,${LINKER_SCRIPT} ${USER_OUT_DIR}/hello.o_embed ${USER_OUT_DIR}/usercrash.o_embed"
        SUFFIX ""
)

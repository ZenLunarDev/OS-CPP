cmake_minimum_required(VERSION 3.20)

# ---- Toolchain: หา i686-elf cross compiler หรือ fallback เป็น host gcc -m32 ----
# (ถ้าจะระบุ toolchain file เอง: cmake -B build -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686.cmake)
if(NOT CMAKE_TOOLCHAIN_FILE)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain-i686.cmake)
endif()

# ★ ต้อง set ก่อน project(): CMAKE_ASM_NASM_OBJECT_FORMAT ถูกอ่านตอน enable language ASM_NASM
# default ของ CMake คือ bare 'elf' ซึ่ง nasm บนเครื่อง 64-bit (Ubuntu CI) ตีความเป็น elf64
# → .asm.obj กลายเป็น ELF x86-64 ทั้งที่ encode เป็น 32-bit แล้ว link ไม่ผ่าน
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)

project(MeowOS C CXX ASM_NASM)

set(COMMON_FLAGS "-m32 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector")
# -fno-pie/-fno-stack-protector: distro gcc (Ubuntu CI) เปิด PIE + stack protector เป็น default
# ซึ่งพังทันทีกับ -nostdlib bare-metal link — cross gcc ไม่มี default พวกนี้ แต่ใส่เพื่อความชัวร์
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COMMON_FLAGS} -fno-exceptions -fno-rtti")
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
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f elf32 ${CMAKE_CURRENT_SOURCE_DIR}/user/user_hello.asm -o hello_user.o
        COMMAND ${MEOWOS_LD} -m elf_i386 -T ${USER_LD_SCRIPT} -o hello.elf hello_user.o
        COMMAND ${MEOWOS_OBJCOPY} -O binary hello.elf hello.bin
        COMMAND ${MEOWOS_OBJCOPY} -I binary -O elf32-i386 -B i386 hello.bin hello.o_embed
        DEPENDS user/user_hello.asm user/user.ld
        WORKING_DIRECTORY ${USER_OUT_DIR}
        COMMENT "Building user program: hello"
)

# usercrash: nasm -f bin ตรงๆ → ELF object สำหรับ embed
add_custom_command(
        OUTPUT ${USER_OUT_DIR}/usercrash.o_embed
        COMMAND ${CMAKE_ASM_NASM_COMPILER} -f bin ${CMAKE_CURRENT_SOURCE_DIR}/user/usercrash.asm -o usercrash.bin
        COMMAND ${MEOWOS_OBJCOPY} -I binary -O elf32-i386 -B i386 usercrash.bin usercrash.o_embed
        DEPENDS user/usercrash.asm
        WORKING_DIRECTORY ${USER_OUT_DIR}
        COMMENT "Building user program: usercrash"
)

add_custom_target(user_programs DEPENDS ${USER_OUT_DIR}/hello.o_embed ${USER_OUT_DIR}/usercrash.o_embed)

add_executable(kernel.bin ${SOURCES})
add_dependencies(kernel.bin user_programs)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker.ld")
set_target_properties(kernel.bin PROPERTIES
        LINK_FLAGS "-m32 -no-pie -nostdlib -Wl,--build-id=none -Wl,-T,${LINKER_SCRIPT} ${USER_OUT_DIR}/hello.o_embed ${USER_OUT_DIR}/usercrash.o_embed"
        SUFFIX ""
)

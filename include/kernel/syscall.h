// syscall.h
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYS_WRITE    1
#define SYS_EXIT     2
#define SYS_GETTICKS 3

extern "C" {
void init_syscall();
// arg4 = caller ring (3 = user, 0 = kernel) — ใช้ validate pointer
uint32_t syscall_handler(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t caller_ring);
void set_user_kernel_esp(uint32_t esp);
}

#endif
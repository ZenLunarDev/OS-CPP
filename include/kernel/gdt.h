#ifndef GDT_H
#define GDT_H

#include <stdint.h>

namespace GDT {
    void init();
}

extern "C" void set_kernel_stack(uint32_t stack);

#endif
#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>

extern "C" {
void heap_init();
void* kmalloc(size_t size);
void kfree(void* ptr);
}

#endif
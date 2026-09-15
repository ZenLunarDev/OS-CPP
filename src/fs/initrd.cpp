#include "initrd.h"
#include <stdint.h>
#include <stddef.h>

static uintptr_t initrd_location_base = 0;

void init_initrd(uintptr_t location) {
    initrd_location_base = location;
}

uint32_t initrd_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node) return 0;

    // Casting location จาก node โดยตรง ป้องกันการเรียก inode ที่ไม่มีอยู่จริง
    struct initrd_file_header* header = (struct initrd_file_header*)(node);

    if (offset > header->length) return 0;
    if (offset + size > header->length) size = header->length - offset;

    uint8_t* src = (uint8_t*)(initrd_location_base + header->offset + offset);

    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = src[i];
    }

    return size;
}

vfs_node* initrd_finddir(vfs_node* node, const char* name) {
    (void)node;
    (void)name;
    return NULL;
}
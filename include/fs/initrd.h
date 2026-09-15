#ifndef INITRD_H
#define INITRD_H

#include "vfs.h"

struct initrd_header {
    uint32_t nfiles;
};

struct initrd_file_header {
    uint8_t magic; // 0xBF
    char name[64];
    uint32_t offset;
    uint32_t length;
};

vfs_node* initialise_initrd(uint32_t location);

#endif
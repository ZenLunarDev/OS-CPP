#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02

struct vfs_node;

typedef uint32_t (*read_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef struct vfs_node* (*finddir_type_t)(struct vfs_node*, const char* name);

struct vfs_node {
    char name[128];
    uint32_t flags;
    uint32_t length;
    read_type_t read;
    write_type_t write;
    finddir_type_t finddir;
    struct vfs_node* ptr; // Custom FS implementation pointer
};

extern vfs_node* fs_root;

uint32_t vfs_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
vfs_node* vfs_finddir(vfs_node* node, const char* name);

// Demo helpers สำหรับ shell
void VFS_init();
void VFS_dump();
void VFS_read_file(int index);

#endif
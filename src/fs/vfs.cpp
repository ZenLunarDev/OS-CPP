#include "vfs.h"

extern "C" void vga_puts(const char* str);

vfs_node* fs_root = nullptr;

// ---- Demo FS: ramdisk เล็กๆ สำหรับ shell ----
static const char* demo_files[] = {
    "hello.txt",
    "readme.txt",
};

static const char* demo_contents[] = {
    "Hello World from MeowOS File System!",
    "MeowOS v0.2 - Kernel is now fully operational",
};

#define DEMO_FILE_COUNT 2

static vfs_node demo_nodes[DEMO_FILE_COUNT];

static uint32_t demo_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    int idx = node - demo_nodes;
    if (idx < 0 || idx >= DEMO_FILE_COUNT) return 0;

    const char* content = demo_contents[idx];
    uint32_t len = 0;
    while (content[len]) len++;

    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = content[offset + i];
    }
    return size;
}

void VFS_init() {
    for (int i = 0; i < DEMO_FILE_COUNT; i++) {
        // manual init: name copy + flags + read hook
        const char* name = demo_files[i];
        uint32_t j = 0;
        for (; name[j] && j < sizeof(demo_nodes[i].name) - 1; j++) {
            demo_nodes[i].name[j] = name[j];
        }
        demo_nodes[i].name[j] = '\0';
        demo_nodes[i].flags = FS_FILE;
        demo_nodes[i].read = demo_read;
        demo_nodes[i].write = nullptr;
        demo_nodes[i].finddir = nullptr;
        demo_nodes[i].ptr = nullptr;
        demo_nodes[i].length = 0;
    }
    fs_root = demo_nodes;
}

void VFS_dump() {
    vga_puts("\nFiles in /:\n");
    for (int i = 0; i < DEMO_FILE_COUNT; i++) {
        vga_puts(demo_nodes[i].name);
        vga_puts("  ");
    }
}

void VFS_read_file(int index) {
    if (index < 0 || index >= DEMO_FILE_COUNT) return;

    uint8_t buffer[128];
    uint32_t n = vfs_read(&demo_nodes[index], 0, sizeof(buffer) - 1, buffer);
    buffer[n] = '\0';

    vga_puts("\n");
    vga_puts(reinterpret_cast<const char*>(buffer));
}

uint32_t vfs_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

uint32_t vfs_write(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node && node->write) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

vfs_node* vfs_finddir(vfs_node* node, const char* name) {
    if (node && (node->flags & FS_DIRECTORY) && node->finddir) {
        return node->finddir(node, name);
    }
    return nullptr;
}
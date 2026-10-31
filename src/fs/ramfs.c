#include "fs/ramfs.h"
#include "arch/lock.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/* Internal RamFS node structure */
typedef struct ramfs_entry {
    vfs_node_t vfs_node;
    uint8_t*   data;
    uint32_t   capacity;
    struct ramfs_entry* parent;
    struct ramfs_entry* children[RAMFS_MAX_FILES_PER_DIR];
    uint32_t   child_count;
} ramfs_entry_t;

static ramfs_entry_t* s_ramfs_root = NULL;
static uint32_t s_next_inode = 100;
static vfs_dirent_t s_shared_dirent;

/* Forward declarations of driver callbacks */
static uint32_t ramfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
static uint32_t ramfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
static vfs_dirent_t* ramfs_readdir(vfs_node_t* node, uint32_t index);
static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name);
static int ramfs_create(vfs_node_t* parent, const char* name, uint32_t flags);
static int ramfs_unlink(vfs_node_t* parent, const char* name);

/*
 * ramfs_alloc_entry:
 * Allocates and initializes an in-memory filesystem entry.
 */
static ramfs_entry_t* ramfs_alloc_entry(const char* name, uint32_t flags) {
    ramfs_entry_t* entry = (ramfs_entry_t*)kmalloc(sizeof(ramfs_entry_t));
    if (!entry) {
        return NULL;
    }

    memset(entry, 0, sizeof(ramfs_entry_t));
    strncpy(entry->vfs_node.name, name, VFS_NAME_MAX - 1);
    entry->vfs_node.flags = flags;
    entry->vfs_node.inode = s_next_inode++;
    entry->vfs_node.length = 0;
    entry->vfs_node.mask = 0777;
    entry->vfs_node.private_data = entry;
    entry->vfs_node.ref_count = 0;

    /* Bind driver operations */
    entry->vfs_node.read = ramfs_read;
    entry->vfs_node.write = ramfs_write;
    entry->vfs_node.readdir = ramfs_readdir;
    entry->vfs_node.finddir = ramfs_finddir;
    entry->vfs_node.create = ramfs_create;
    entry->vfs_node.unlink = ramfs_unlink;

    if (flags & VFS_FILE) {
        entry->capacity = RAMFS_DEFAULT_CAPACITY;
        entry->data = (uint8_t*)kmalloc(entry->capacity);
        if (entry->data) {
            memset(entry->data, 0, entry->capacity);
        }
    }

    return entry;
}

/*
 * ramfs_read:
 * Reads data from an in-memory file.
 */
static uint32_t ramfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (!node || !buffer || !(node->flags & VFS_FILE)) {
        return 0;
    }

    ramfs_entry_t* entry = (ramfs_entry_t*)node->private_data;
    if (!entry || !entry->data) {
        return 0;
    }

    if (offset >= node->length) {
        return 0;
    }

    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > node->length) {
        bytes_to_read = node->length - offset;
    }

    memcpy(buffer, entry->data + offset, bytes_to_read);
    return bytes_to_read;
}

/*
 * ramfs_write:
 * Writes data into an in-memory file, dynamically growing buffer if needed.
 */
static uint32_t ramfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, const uint8_t* buffer) {
    if (!node || !buffer || !(node->flags & VFS_FILE)) {
        return 0;
    }

    /* Security Hardening: Guard against integer wrap-around & oversized allocations */
    if (offset + size < offset || size > 0x10000000) {
        return 0;
    }

    ramfs_entry_t* entry = (ramfs_entry_t*)node->private_data;
    if (!entry) {
        return 0;
    }

    uint32_t required_capacity = offset + size + 1;
    if (required_capacity > entry->capacity) {
        uint32_t new_cap = entry->capacity ? entry->capacity * 2 : RAMFS_DEFAULT_CAPACITY;
        while (new_cap < required_capacity) {
            new_cap *= 2;
        }

        uint8_t* new_data = (uint8_t*)kmalloc(new_cap);
        if (!new_data) {
            return 0;
        }

        memset(new_data, 0, new_cap);
        if (entry->data && node->length > 0) {
            memcpy(new_data, entry->data, node->length);
            kfree(entry->data);
        }
        entry->data = new_data;
        entry->capacity = new_cap;
    }

    memcpy(entry->data + offset, buffer, size);
    if (offset + size > node->length) {
        node->length = offset + size;
    }

    /* Ensure null-terminator for text safety */
    if (entry->data && node->length < entry->capacity) {
        entry->data[node->length] = '\0';
    }

    return size;
}

/*
 * ramfs_readdir:
 * Enumerates entries within a directory node.
 */
static vfs_dirent_t* ramfs_readdir(vfs_node_t* node, uint32_t index) {
    if (!node || !(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }

    ramfs_entry_t* entry = (ramfs_entry_t*)node->private_data;
    if (!entry || index >= entry->child_count) {
        return NULL;
    }

    ramfs_entry_t* child = entry->children[index];
    if (!child) {
        return NULL;
    }

    strncpy(s_shared_dirent.name, child->vfs_node.name, VFS_NAME_MAX - 1);
    s_shared_dirent.name[VFS_NAME_MAX - 1] = '\0';
    s_shared_dirent.inode = child->vfs_node.inode;
    s_shared_dirent.type = child->vfs_node.flags;
    s_shared_dirent.size = child->vfs_node.length;

    return &s_shared_dirent;
}

/*
 * ramfs_finddir:
 * Looks up a child node by name within a directory.
 */
static vfs_node_t* ramfs_finddir(vfs_node_t* node, const char* name) {
    if (!node || !name || !(node->flags & VFS_DIRECTORY)) {
        return NULL;
    }

    ramfs_entry_t* entry = (ramfs_entry_t*)node->private_data;
    if (!entry) {
        return NULL;
    }

    for (uint32_t i = 0; i < entry->child_count; i++) {
        if (entry->children[i] && strcmp(entry->children[i]->vfs_node.name, name) == 0) {
            return &entry->children[i]->vfs_node;
        }
    }

    return NULL;
}

/*
 * ramfs_create:
 * Creates a new child file or directory entry.
 */
static int ramfs_create(vfs_node_t* parent, const char* name, uint32_t flags) {
    if (!parent || !name || !(parent->flags & VFS_DIRECTORY)) {
        return -1;
    }

    ramfs_entry_t* parent_entry = (ramfs_entry_t*)parent->private_data;
    if (!parent_entry || parent_entry->child_count >= RAMFS_MAX_FILES_PER_DIR) {
        return -1;
    }

    /* Check if already exists */
    if (ramfs_finddir(parent, name) != NULL) {
        return 0; /* Already exists */
    }

    ramfs_entry_t* child = ramfs_alloc_entry(name, flags);
    if (!child) {
        return -1;
    }

    child->parent = parent_entry;
    parent_entry->children[parent_entry->child_count++] = child;
    return 0;
}

/*
 * ramfs_unlink:
 * Removes and deallocates a child file.
 */
static int ramfs_unlink(vfs_node_t* parent, const char* name) {
    if (!parent || !name || !(parent->flags & VFS_DIRECTORY)) {
        return -1;
    }

    ramfs_entry_t* parent_entry = (ramfs_entry_t*)parent->private_data;
    if (!parent_entry) {
        return -1;
    }

    int target_idx = -1;
    for (uint32_t i = 0; i < parent_entry->child_count; i++) {
        if (parent_entry->children[i] && strcmp(parent_entry->children[i]->vfs_node.name, name) == 0) {
            target_idx = (int)i;
            break;
        }
    }

    if (target_idx < 0) {
        return -1;
    }

    ramfs_entry_t* target = parent_entry->children[target_idx];

    /* Shift remaining entries */
    for (uint32_t i = (uint32_t)target_idx; i < parent_entry->child_count - 1; i++) {
        parent_entry->children[i] = parent_entry->children[i + 1];
    }
    parent_entry->children[--parent_entry->child_count] = NULL;

    /* Free target buffers */
    if (target->data) {
        kfree(target->data);
    }
    kfree(target);

    return 0;
}

/*
 * ramfs_create_file:
 * Convenience helper to create a file in root with initial text content.
 */
int ramfs_create_file(const char* name, const char* initial_content) {
    if (!s_ramfs_root || !name) {
        return -1;
    }

    if (ramfs_create(&s_ramfs_root->vfs_node, name, VFS_FILE) != 0) {
        return -1;
    }

    if (initial_content) {
        vfs_node_t* node = ramfs_finddir(&s_ramfs_root->vfs_node, name);
        if (node) {
            size_t len = strlen(initial_content);
            ramfs_write(node, 0, (uint32_t)len, (const uint8_t*)initial_content);
        }
    }

    return 0;
}

/*
 * ramfs_init:
 * Initializes and mounts RamFS as the system root filesystem ("/").
 */
vfs_node_t* ramfs_init(void) {
    s_ramfs_root = ramfs_alloc_entry("", VFS_DIRECTORY);
    if (!s_ramfs_root) {
        kprintf("[RAMFS] Error: Failed to allocate root directory!\n");
        return NULL;
    }

    vfs_mount_root(&s_ramfs_root->vfs_node);

    /* Pre-seed root directory with default files */
    ramfs_create_file("welcome.txt",
        "=====================================================\n"
        "  Welcome to MyOS 32-bit Graphical Operating System  \n"
        "=====================================================\n\n"
        "Features:\n"
        " - Linear Framebuffer (1024x768x32 True Color)\n"
        " - Preemptive Round-Robin Multitasking Scheduler\n"
        " - Virtual File System (VFS) & In-Memory RamFS\n"
        " - Graphical Desktop, File Explorer & Task Manager\n"
        " - Interactive CLI Terminal with Shell Commands\n\n"
        "Enjoy testing your operating system!\n");

    ramfs_create_file("readme.txt",
        "MyOS Final Year B.Tech Project\n"
        "Author: Pruthviraj Vinod Phuse\n"
        "Architecture: x86 32-bit Protected Mode (i686)\n"
        "Toolchain: i686-elf-gcc, NASM, GNU Make\n"
        "Virtual Machine: VirtualBox (OSProject)\n");

    ramfs_create_file("system.nfo",
        "OS: MyOS v1.0\n"
        "Arch: i686 Flat 4GB Protected Mode\n"
        "Display: 1024x768x32 VBE Framebuffer\n"
        "Memory: 128 MB RAM (Identity Mapped)\n"
        "Paging: 2-Level Paging (CR0.PG, 4KB pages)\n"
        "Scheduler: Round-Robin (100 Hz, 100 ms quantum)\n"
        "FS: VFS + RamFS\n");

    ramfs_create_file("todo.txt",
        "- [x] Preemptive multitasking scheduler\n"
        "- [x] Virtual file system (VFS)\n"
        "- [x] Graphical File Explorer\n"
        "- [x] Graphical Task Manager\n"
        "- [x] Graphical Terminal Emulator\n"
        "- [ ] PCI Bus scan & Network driver\n"
        "- [ ] Web browser & Sound Blaster 16\n");

    kprintf("[RAMFS] RamFS initialized with 4 pre-seeded documents\n");
    return &s_ramfs_root->vfs_node;
}


#include "fs/vfs.h"
#include "arch/lock.h"
#include "mm/heap.h"
#include "lib/string.h"
#include "lib/kprintf.h"

/* Root VFS node ("/") */
static vfs_node_t* s_vfs_root = NULL;

/* Global open file descriptors table */
static vfs_file_t s_file_table[VFS_MAX_OPEN_FILES];

/*
 * vfs_init:
 * Initializes the Virtual File System manager and file handle tables.
 */
void vfs_init(void) {
    uint32_t flags = irq_save();

    s_vfs_root = NULL;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        s_file_table[i].node = NULL;
        s_file_table[i].offset = 0;
        s_file_table[i].flags = 0;
        s_file_table[i].in_use = false;
    }

    irq_restore(flags);
    kprintf("[VFS] Virtual File System initialized (%u file descriptors)\n", VFS_MAX_OPEN_FILES);
}

/*
 * vfs_mount_root:
 * Mounts the designated filesystem root node at "/".
 */
void vfs_mount_root(vfs_node_t* root_node) {
    uint32_t flags = irq_save();
    s_vfs_root = root_node;
    irq_restore(flags);
    kprintf("[VFS] Mounted root node at '/'\n");
}

/*
 * vfs_get_root:
 * Returns the currently mounted root VFS node.
 */
vfs_node_t* vfs_get_root(void) {
    return s_vfs_root;
}

/*
 * vfs_lookup:
 * Traverses path components from root to resolve a target VFS node.
 */
vfs_node_t* vfs_lookup(const char* path) {
    if (!path || !s_vfs_root) {
        return NULL;
    }

    /* Root path resolution */
    if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
        return s_vfs_root;
    }

    /* Skip leading slash */
    if (*path == '/') {
        path++;
    }

    char token[VFS_NAME_MAX];
    vfs_node_t* curr = s_vfs_root;

    while (*path != '\0') {
        int idx = 0;
        while (*path != '\0' && *path != '/' && idx < (VFS_NAME_MAX - 1)) {
            token[idx++] = *path++;
        }
        token[idx] = '\0';

        if (*path == '/') {
            path++;
        }

        if (idx == 0) {
            continue;
        }

        if (!curr->finddir) {
            return NULL;
        }

        curr = curr->finddir(curr, token);
        if (!curr) {
            return NULL;
        }
    }

    return curr;
}

/*
 * vfs_open:
 * Opens a file by path, returning a file descriptor handle or -1 on error.
 */
int vfs_open(const char* path, uint32_t flags) {
    if (!path) {
        return -1;
    }

    vfs_node_t* node = vfs_lookup(path);
    if (!node) {
        /* Auto-create file if write/create flag requested */
        if (flags & 0x02) {
            if (vfs_create(path, VFS_FILE) == 0) {
                node = vfs_lookup(path);
            }
        }
    }

    if (!node) {
        return -1;
    }

    uint32_t irq_flags = irq_save();

    /* Find an available file descriptor slot */
    int fd = -1;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!s_file_table[i].in_use) {
            fd = i;
            break;
        }
    }

    if (fd < 0) {
        irq_restore(irq_flags);
        kprintf("[VFS] Error: Out of file descriptors!\n");
        return -1;
    }

    s_file_table[fd].node = node;
    s_file_table[fd].offset = 0;
    s_file_table[fd].flags = flags;
    s_file_table[fd].in_use = true;
    node->ref_count++;

    if (node->open) {
        node->open(node);
    }

    irq_restore(irq_flags);
    return fd;
}

/*
 * vfs_close:
 * Closes an open file descriptor.
 */
int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return -1;
    }

    uint32_t irq_flags = irq_save();

    if (!s_file_table[fd].in_use) {
        irq_restore(irq_flags);
        return -1;
    }

    vfs_node_t* node = s_file_table[fd].node;
    if (node) {
        if (node->close) {
            node->close(node);
        }
        if (node->ref_count > 0) {
            node->ref_count--;
        }
    }

    s_file_table[fd].node = NULL;
    s_file_table[fd].offset = 0;
    s_file_table[fd].flags = 0;
    s_file_table[fd].in_use = false;

    irq_restore(irq_flags);
    return 0;
}

/*
 * vfs_read:
 * Reads data from an open file descriptor.
 */
int vfs_read(int fd, void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buffer || size == 0) {
        return -1;
    }

    uint32_t irq_flags = irq_save();

    if (!s_file_table[fd].in_use || !s_file_table[fd].node) {
        irq_restore(irq_flags);
        return -1;
    }

    vfs_node_t* node = s_file_table[fd].node;
    if (!node->read) {
        irq_restore(irq_flags);
        return -1;
    }

    uint32_t bytes_read = node->read(node, s_file_table[fd].offset, size, (uint8_t*)buffer);
    s_file_table[fd].offset += bytes_read;

    irq_restore(irq_flags);
    return (int)bytes_read;
}

/*
 * vfs_write:
 * Writes data to an open file descriptor.
 */
int vfs_write(int fd, const void* buffer, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buffer || size == 0) {
        return -1;
    }

    uint32_t irq_flags = irq_save();

    if (!s_file_table[fd].in_use || !s_file_table[fd].node) {
        irq_restore(irq_flags);
        return -1;
    }

    vfs_node_t* node = s_file_table[fd].node;
    if (!node->write) {
        irq_restore(irq_flags);
        return -1;
    }

    uint32_t bytes_written = node->write(node, s_file_table[fd].offset, size, (const uint8_t*)buffer);
    s_file_table[fd].offset += bytes_written;

    irq_restore(irq_flags);
    return (int)bytes_written;
}

/*
 * vfs_seek:
 * Updates the read/write offset of an open file descriptor.
 */
int vfs_seek(int fd, uint32_t offset) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return -1;
    }

    uint32_t irq_flags = irq_save();

    if (!s_file_table[fd].in_use) {
        irq_restore(irq_flags);
        return -1;
    }

    s_file_table[fd].offset = offset;

    irq_restore(irq_flags);
    return 0;
}

/*
 * vfs_tell_size:
 * Returns the total length in bytes of an open file descriptor.
 */
uint32_t vfs_tell_size(int fd) {
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return 0;
    }

    if (!s_file_table[fd].in_use || !s_file_table[fd].node) {
        return 0;
    }

    return s_file_table[fd].node->length;
}

/*
 * vfs_create:
 * Creates a new file at the specified path.
 */
int vfs_create(const char* path, uint32_t flags) {
    if (!path || !s_vfs_root) {
        return -1;
    }

    /* Extract parent path and basename */
    const char* last_slash = NULL;
    for (const char* p = path; *p != '\0'; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    vfs_node_t* parent = NULL;
    const char* basename = NULL;

    if (!last_slash || last_slash == path) {
        parent = s_vfs_root;
        basename = (last_slash == path) ? (path + 1) : path;
    } else {
        char parent_path[VFS_NAME_MAX];
        size_t len = (size_t)(last_slash - path);
        if (len >= sizeof(parent_path)) len = sizeof(parent_path) - 1;
        strncpy(parent_path, path, len);
        parent_path[len] = '\0';
        parent = vfs_lookup(parent_path);
        basename = last_slash + 1;
    }

    if (!parent || !parent->create) {
        return -1;
    }

    return parent->create(parent, basename, flags);
}

/*
 * vfs_delete:
 * Deletes / unlinks a file at the specified path.
 */
int vfs_delete(const char* path) {
    if (!path || !s_vfs_root) {
        return -1;
    }

    const char* last_slash = NULL;
    for (const char* p = path; *p != '\0'; p++) {
        if (*p == '/') {
            last_slash = p;
        }
    }

    vfs_node_t* parent = NULL;
    const char* basename = NULL;

    if (!last_slash || last_slash == path) {
        parent = s_vfs_root;
        basename = (last_slash == path) ? (path + 1) : path;
    } else {
        char parent_path[VFS_NAME_MAX];
        size_t len = (size_t)(last_slash - path);
        if (len >= sizeof(parent_path)) len = sizeof(parent_path) - 1;
        strncpy(parent_path, path, len);
        parent_path[len] = '\0';
        parent = vfs_lookup(parent_path);
        basename = last_slash + 1;
    }

    if (!parent || !parent->unlink) {
        return -1;
    }

    return parent->unlink(parent, basename);
}

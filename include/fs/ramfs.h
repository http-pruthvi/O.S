#ifndef FS_RAMFS_H
#define FS_RAMFS_H

#include "fs/vfs.h"

/*
 * ramfs.h:
 * In-Memory RAM Filesystem Interface.
 *
 * Why this is needed:
 * Provides a dynamic, read/write filesystem stored in kernel memory.
 * Implements the VFS node operations (read, write, create, unlink, readdir, finddir).
 * Pre-populates the root directory with default documents, documentation,
 * and system information files on startup.
 */

#define RAMFS_MAX_FILES_PER_DIR 64
#define RAMFS_DEFAULT_CAPACITY  1024

/* Initializes and mounts the RamFS as the system root filesystem ("/") */
vfs_node_t* ramfs_init(void);

/* Creates a pre-populated file in the RamFS root directory */
int ramfs_create_file(const char* name, const char* initial_content);

#endif /* FS_RAMFS_H */

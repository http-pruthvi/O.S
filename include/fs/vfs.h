#ifndef FS_VFS_H
#define FS_VFS_H

#include "lib/types.h"

/*
 * vfs.h:
 * Virtual File System (VFS) Abstraction Layer.
 *
 * Why this is needed:
 * An operating system requires a uniform, abstract interface for accessing
 * files and directories, regardless of whether the underlying storage is an
 * in-memory RAM disk (ramfs), an ATA hard disk, or a virtual filesystem.
 * The VFS decouples file operations (open, read, write, close, readdir) from
 * filesystem-specific on-disk structures.
 */

#define VFS_NAME_MAX       64
#define VFS_MAX_OPEN_FILES 32

/* Node Type Flags */
#define VFS_FILE           0x01
#define VFS_DIRECTORY      0x02
#define VFS_CHARDEVICE     0x03
#define VFS_BLOCKDEVICE    0x04

/* Forward declarations */
struct vfs_node;
struct vfs_dirent;

/* Function pointer types for filesystem driver callbacks */
typedef uint32_t (*vfs_read_fn_t)(struct vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef uint32_t (*vfs_write_fn_t)(struct vfs_node* node, uint32_t offset, uint32_t size, const uint8_t* buffer);
typedef void (*vfs_open_fn_t)(struct vfs_node* node);
typedef void (*vfs_close_fn_t)(struct vfs_node* node);
typedef struct vfs_dirent* (*vfs_readdir_fn_t)(struct vfs_node* node, uint32_t index);
typedef struct vfs_node* (*vfs_finddir_fn_t)(struct vfs_node* node, const char* name);
typedef int (*vfs_create_fn_t)(struct vfs_node* parent, const char* name, uint32_t flags);
typedef int (*vfs_delete_fn_t)(struct vfs_node* parent, const char* name);

/*
 * Directory Entry structure returned by vfs_readdir
 */
typedef struct vfs_dirent {
    char name[VFS_NAME_MAX];    /* File or directory name */
    uint32_t inode;             /* Inode number */
    uint32_t type;              /* VFS_FILE, VFS_DIRECTORY, etc. */
    uint32_t size;              /* File size in bytes */
} vfs_dirent_t;

/*
 * VFS Inode / Node structure representing an openable file or directory
 */
typedef struct vfs_node {
    char name[VFS_NAME_MAX];    /* Node basename */
    uint32_t flags;             /* Type flags (VFS_FILE, VFS_DIRECTORY) */
    uint32_t inode;             /* Unique inode identifier */
    uint32_t length;            /* File length / size in bytes */
    uint32_t mask;              /* Permission bitmask */
    uint32_t uid;               /* Owner User ID */
    uint32_t gid;               /* Owner Group ID */

    /* Driver operation callbacks */
    vfs_read_fn_t    read;
    vfs_write_fn_t   write;
    vfs_open_fn_t    open;
    vfs_close_fn_t   close;
    vfs_readdir_fn_t readdir;
    vfs_finddir_fn_t finddir;
    vfs_create_fn_t  create;
    vfs_delete_fn_t  unlink;

    /* Filesystem-specific private data handle */
    void* private_data;

    /* Reference count for open handles */
    uint32_t ref_count;
} vfs_node_t;

/*
 * File Handle structure for tracking an active open file session
 */
typedef struct vfs_file {
    vfs_node_t* node;           /* Underlying VFS node */
    uint32_t offset;            /* Current read/write seek position */
    uint32_t flags;             /* Access mode flags */
    bool in_use;                /* True if this handle descriptor is open */
} vfs_file_t;

/* Initializes the Virtual File System manager */
void vfs_init(void);

/* Mounts a root node at the filesystem root ("/") */
void vfs_mount_root(vfs_node_t* root_node);

/* Returns the root VFS node */
vfs_node_t* vfs_get_root(void);

/* Resolves an absolute path (e.g. "/welcome.txt") to its corresponding VFS node */
vfs_node_t* vfs_lookup(const char* path);

/* Opens a file by path, returning a file descriptor (> 0) or -1 on error */
int vfs_open(const char* path, uint32_t flags);

/* Closes an open file descriptor */
int vfs_close(int fd);

/* Reads up to `size` bytes from the file descriptor into `buffer` */
int vfs_read(int fd, void* buffer, uint32_t size);

/* Writes up to `size` bytes from `buffer` to the file descriptor */
int vfs_write(int fd, const void* buffer, uint32_t size);

/* Seeks the file descriptor offset */
int vfs_seek(int fd, uint32_t offset);

/* Returns the total length of an open file descriptor */
uint32_t vfs_tell_size(int fd);

/* Creates a new file at the specified path */
int vfs_create(const char* path, uint32_t flags);

/* Deletes / unlinks a file at the specified path */
int vfs_delete(const char* path);

#endif /* FS_VFS_H */


/// @file pipe.c
/// @brief PIPE functions and structures implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// ============================================================================
// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PIPE  ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.
// ============================================================================

#include "fs/pipe.h"

#include "assert.h"
#include "fcntl.h"
#include "mem/kheap.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "sys/errno.h"
#include "sys/list_head.h"
#include "fs/vfs.h"
#include "sys/stat.h"
#include "fcntl.h"

/// @brief This constant specifies the size of the buffer allocated for each
/// pipe, and its value can affect the performance and capacity of pipes.
#define PIPE_BUF (PAGE_SIZE * 4)

/// @brief The number of initial buffers.
#define INITIAL_NUM_BUFFERS 1

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

static int pipe_buffer_confirm(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf);
static void pipe_buffer_release(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf);
static int pipe_buffer_try_steal(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf);
static int pipe_buffer_get(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf);

static vfs_file_t *pipe_open(const char *path, int flags, mode_t mode);
static int pipe_close(vfs_file_t *file);
static ssize_t pipe_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t pipe_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);

static const struct pipe_buf_operations pipe_buf_ops = {
    .confirm   = pipe_buffer_confirm,
    .release   = pipe_buffer_release,
    .try_steal = pipe_buffer_try_steal,
    .get       = pipe_buffer_get,
};

/// Filesystem general operations.
static const vfs_sys_operations_t pipe_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = NULL,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static const vfs_file_operations_t pipe_fs_operations = {
    .open_f     = pipe_open,
    .unlink_f   = NULL,
    .close_f    = pipe_close,
    .read_f     = pipe_read,
    .write_f    = pipe_write,
    .lseek_f    = NULL,
    .stat_f     = NULL,
    .ioctl_f    = NULL,
    .getdents_f = NULL,
    .readlink_f = NULL,
};

// ============================================================================
// MEMORY MANAGEMENT (Private)
// ============================================================================

/// @brief Initializes a pipe buffer.
/// @details This function sets the initial values for a `pipe_buffer_t`
/// structure, and allocates a memory page to hold the buffer's data.
static inline int __pipe_buffer_init(pipe_buffer_t *pipe_buffer)
{
    // Check if we received a valid pipe buffer.
    assert(pipe_buffer && "Received a null pipe buffer.");
    // Initialize the pipe buffer.
    memset(pipe_buffer, 0, sizeof(pipe_buffer_t));
    // Determine the appropriate order for page allocation.
    uint32_t order = find_nearest_order_greater(0, PIPE_BUF);
    // Allocate a memory page for storing the buffer's data.
    pipe_buffer->page = _alloc_pages(GFP_KERNEL, order);
    if (pipe_buffer->page == NULL) {
        pr_err("Failed to allocate pages for a pipe buffer.\n");
        return 1;
    }
    // Initialize additional fields in the pipe_buffer_t structure.
    pipe_buffer->len    = 0;
    pipe_buffer->offset = 0;
    pipe_buffer->ops    = &pipe_buf_ops;
    return 0;
}

/// @brief De-initialize a pipe buffer.
/// @details This function frees the memory pages used to store the buffer's
/// data.
/// @param pipe_buffer Pointer to the `pipe_buffer_t` structure to be
/// de-initialize. This should not be NULL.
static inline void __pipe_buffer_deinit(pipe_buffer_t *pipe_buffer)
{
    // Check if we received a valid pipe buffer.
    assert(pipe_buffer && "Received a null pipe buffer.");
    // Free the memory page allocated for the buffer's data.
    if (pipe_buffer->page) {
        __free_pages(pipe_buffer->page);
    }
}

/// @brief Allocates and initializes a new `pipe_inode_info_t` structure.
/// @details This function allocates memory for a `pipe_inode_info_t` structure
/// and initializes its fields to default values. It also sets up the wait queue
/// and mutex for process synchronization. The buffer array is allocated with an
/// initial size based on the `PIPE_BUF` constant, which determines the maximum
/// buffer size.
/// @return Pointer to the allocated and initialized `pipe_inode_info_t`
/// structure. NULL if the allocation fails.
static inline pipe_inode_info_t *__pipe_inode_info_alloc(void)
{
    // Allocate memory for the pipe_inode_info_t structure.
    pipe_inode_info_t *pipe_info = (pipe_inode_info_t *)kmalloc(sizeof(pipe_inode_info_t));
    // Check if memory allocation was successful.
    if (!pipe_info) {
        pr_err("Failed to allocate memory for the pipe info.");
        return NULL;
    }
    // Initialize the allocated memory to zero.
    memset(pipe_info, 0, sizeof(pipe_inode_info_t));
    // Initialize the wait queue.
    spinlock_init(&pipe_info->wait.lock);
    list_head_init(&pipe_info->wait.task_list);
    // Initialize the mutex.
    mutex_unlock(&pipe_info->mutex);
    // Allocate memory for the array of pipe buffers.
    pipe_info->numbuf = INITIAL_NUM_BUFFERS;
    pipe_info->bufs   = (pipe_buffer_t *)kmalloc(pipe_info->numbuf * sizeof(pipe_buffer_t));
    // Check if the buffer array allocation was successful.
    if (!pipe_info->bufs) {
        pr_err("Failed to allocate memory for the buffers.");
        // Free the pipe_info structure if buffer allocation fails.
        kfree(pipe_info);
        return NULL;
    }
    // Initialize the buffer array (e.g., by setting up each buffer).
    for (unsigned int i = 0; i < pipe_info->numbuf; ++i) {
        // Initialize each buffer.
        if (__pipe_buffer_init(pipe_info->bufs + i)) {
            // Free already allocated buffers if initialization fails.
            for (unsigned int j = 0; j < i; ++j) {
                __pipe_buffer_deinit(pipe_info->bufs + j);
            }
            kfree(pipe_info->bufs);
            kfree(pipe_info);
            return NULL;
        }
    }
    // Set the default values for other fields.
    pipe_info->curbuf  = 0;
    pipe_info->readers = 0;
    pipe_info->writers = 0;
    // Return the allocated and initialized structure.
    return pipe_info;
}

/// @brief Deallocates the memory used by a `pipe_inode_info_t` structure.
/// @details This function frees the memory associated with a
/// `pipe_inode_info_t` structure, including the array of pipe buffers and the
/// structure itself. It also ensures that all allocated resources are properly
/// released.
/// @param pipe_info Pointer to the `pipe_inode_info_t` structure to be
/// deallocated. This should not be NULL.
static inline void __pipe_inode_info_dealloc(pipe_inode_info_t *pipe_info)
{
    // Ensure that the provided pointer is not NULL.
    assert(pipe_info && "Received a NULL pointer.");
    // Free each buffer in the array.
    for (unsigned int i = 0; i < pipe_info->numbuf; ++i) {
        // Deallocate each buffer. Handle failures gracefully in real scenarios.
        __pipe_buffer_deinit(pipe_info->bufs + i);
    }
    // Free the memory used for the array of pipe buffers.
    kfree(pipe_info->bufs);
    // Free the memory used by the pipe_inode_info_t structure itself.
    kfree(pipe_info);
}

// ============================================================================
// BUFFER OPERATIONS (Private)
// ============================================================================

/// @brief Ensures that the buffer is valid and ready to be used.
/// @param pipe_info Pointer to the pipe information structure.
/// @param buf Pointer to the pipe buffer to be confirmed.
/// @return 0 if the buffer is valid, or a non-zero error code if it is not.
static int pipe_buffer_confirm(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf)
{
    // Ensure the pipe_info and buffer are valid.
    if (!pipe_info || !buf || !buf->page) {
        return -EINVAL;
    }
    return 0;
}

/// @brief Handles cleanup and release operations for a buffer.
/// @param pipe_info Pointer to the pipe information structure.
/// @param buf Pointer to the pipe buffer to be released.
static void pipe_buffer_release(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf)
{
    // Ensure the pipe_info and buffer are valid.
    if (pipe_info && buf && buf->page) {
        // Release the buffer's resources.
        if (!buf->offset && !buf->len) {
            __pipe_buffer_deinit(buf);
        }
    }
}

/// @brief Attempts to take ownership of the buffer for zero-copy operations.
/// @param pipe_info Pointer to the pipe information structure.
/// @param buf Pointer to the pipe buffer to be stolen.
/// @return 0 if the buffer was successfully stolen, or a non-zero error code if not.
static int pipe_buffer_try_steal(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf)
{
    // Ensure the pipe_info and buffer are valid.
    if (!pipe_info || !buf || !buf->page) {
        return -EINVAL;
    }
    return 0;
}

/// @brief Increments the reference count for the buffer to prevent it from
/// being freed or altered while being accessed.
/// @param pipe_info Pointer to the pipe information structure.
/// @param buf Pointer to the pipe buffer to be accessed.
/// @return 0 if the buffer was successfully acquired, or a non-zero error code if not.
static int pipe_buffer_get(pipe_inode_info_t *pipe_info, pipe_buffer_t *buf)
{
    // Ensure the pipe_info and buffer are valid.
    if (!pipe_info || !buf || !buf->page) {
        return -EINVAL;
    }
    return 0;
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Creates a VFS file.
/// @param procfs_file the PROCFS file.
/// @return a pointer to the newly create VFS file, NULL on failure.
static inline vfs_file_t *pipe_create_file_struct(void)
{
    vfs_file_t *vfs_file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!vfs_file) {
        pr_err("Failed to allocate memory for VFS file!\n");
        return NULL;
    }
    memset(vfs_file, 0, sizeof(vfs_file_t));
    // vfs_file->name;
    // vfs_file->device;
    vfs_file->mask = S_IRUSR | S_IRGRP | S_IROTH;
    // vfs_file->uid;
    // vfs_file->gid;
    // vfs_file->flags;
    // vfs_file->ino;
    // vfs_file->length;
    // vfs_file->impl;
    // vfs_file->open_flags;
    // vfs_file->count;
    // vfs_file->atime;
    // vfs_file->mtime;
    // vfs_file->ctime;
    vfs_file->sys_operations = &pipe_sys_operations;
    vfs_file->fs_operations  = &pipe_fs_operations;
    // vfs_file->f_pos;
    // vfs_file->nlink;
    list_head_init(&vfs_file->siblings);
    // vfs_file->refcount;
    return vfs_file;
}

static vfs_file_t *pipe_open(const char *path, int flags, mode_t mode)
{
    return NULL;
}

static int pipe_close(vfs_file_t *file)
{
    return -1;
}

static ssize_t pipe_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    return -1;
}

static ssize_t pipe_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte)
{
    return -1;
}

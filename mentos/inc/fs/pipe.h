/// @file pipe.h
/// @brief PIPE functions and structures declaration.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "process/wait.h"
#include "mem/paging.h"
#include "klib/mutex.h"

/// @brief Represents a single buffer within a pipe. This structure manages the
/// data stored in the buffer, including its memory location, size, and
/// associated operations.
typedef struct pipe_buffer {
    /// Pointer to the memory page where the buffer's data is stored. This page
    /// may be shared or unique depending on the buffer type.
    page_t *page;

    /// Offset within the memory page where the buffer's data begins. This
    /// allows for partial usage of the page if the buffer does not occupy the
    /// entire page.
    unsigned int offset;

    /// Length of the data currently stored in the buffer. This indicates the
    /// amount of data that the buffer holds and can be used during read and
    /// write operations.
    unsigned int len;

    /// Pointer to a set of operations that can be performed on the buffer.
    /// These operations include functions for getting, releasing, and mapping
    /// the buffer, tailored to the specific needs of the buffer's data type.
    const struct pipe_buf_operations *ops;
} pipe_buffer_t;

/// @brief This structure represents a pipe in the kernel. It contains
/// information about the buffer used for the pipe, the readers and writers, and
/// synchronization details.
typedef struct pipe_inode_info {
    /// Array of pointers to pipe buffers. Each buffer holds a portion of data
    /// for the pipe.
    /// @note: For scalability and performance, multiple buffers SHOULD BE used
    /// to handle data efficiently.
    pipe_buffer_t *bufs;

    /// Number of buffers allocated for the pipe. This value determines the size
    /// of the `bufs` array and how many buffers are available for use.
    unsigned int numbuf;

    /// Index of the buffer that is currently being accessed or modified.
    unsigned int curbuf;

    /// The number of processes currently reading from the pipe.
    unsigned int readers;

    /// The number of processes currently writing to the pipe.
    unsigned int writers;

    /// Wait queue for processes that are blocked waiting to read from or write
    /// to the pipe. This queue helps manage process scheduling and
    /// synchronization.
    wait_queue_head_t wait;

    /// Mutex for protecting access to the pipe structure and ensuring
    /// thread-safe operations. This prevents race conditions when multiple
    /// processes or threads interact with the pipe.
    mutex_t mutex;
} pipe_inode_info_t;

/// @brief Structure defining operations for managing pipe buffers.
/// @details
/// This structure contains function pointers to operations that can be
/// performed on pipe buffers. These operations handle various aspects of buffer
/// management including validation, reference counting, buffer stealing, and
/// releasing. Each buffer in a pipe is associated with a set of these
/// operations, which allows the kernel to manage different types of buffers
/// efficiently and safely.
typedef struct pipe_buf_operations {
    /// @brief Ensures that the buffer is valid and ready to be used. This might
    /// involve checking if the buffer contains valid data or if any required
    /// synchronizations are in place.
    int (*confirm)(pipe_inode_info_t *, pipe_buffer_t *);

    /// @brief Handles the cleanup and release of the buffer when it is no
    /// longer in use. This function may decrease reference counts or perform
    /// other necessary actions to free or repurpose the buffer.
    void (*release)(pipe_inode_info_t *, pipe_buffer_t *);

    /// @brief Attempts to take ownership of the buffer. This is useful for
    /// scenarios where you want to transfer the buffer's data to another
    /// component or process without copying it, such as in zero-copy
    /// operations.
    int (*try_steal)(pipe_inode_info_t *, pipe_buffer_t *);

    /// @brief Increments the reference count for the buffer, ensuring that it
    /// is not freed or altered while being accessed. This is crucial for
    /// maintaining data integrity when multiple readers or writers are
    /// interacting with the buffer.
    int (*get)(pipe_inode_info_t *, pipe_buffer_t *);
} pipe_buf_operations_t;

/// @file devfs.h
/// @brief Proc file system public functions and structures.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "fs/vfs_types.h"

/// @brief Stores information about a devfs directory entry.
typedef struct devfs_dir_entry_t {
    /// Generic system operations.
    vfs_sys_operations_t *sys_operations;
    /// Files operations.
    vfs_file_operations_t *fs_operations;
    /// Data associated with the dir_entry.
    void *data;
    /// Name of the entry.
    const char *name;
} devfs_dir_entry_t;

/// @brief Initialize the devfs filesystem.
/// @return 0 if fails, 1 if succeed.
int devfs_module_init(void);

/// @brief Clean up the devfs filesystem.
/// @return 0 if fails, 1 if succeed.
int devfs_cleanup_module(void);

/// @brief Finds the direntry inside `/dev`.
/// @param name   The name of the entry we are searching.
/// @return A pointer to the entry, or NULL.
devfs_dir_entry_t *devfs_dir_entry_get(const char *name);

/// @brief Creates a new entry inside `/dev`.
/// @param name   The name of the entry we are creating.
/// @return A pointer to the entry, or NULL if fails.
devfs_dir_entry_t *devfs_create_entry(const char *name);

/// @brief Removes an entry from `/dev`.
/// @param name   The name of the entry we are removing.
int devfs_destroy_entry(const char *name);

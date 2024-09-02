/// @file devfs.c
/// @brief Devfs file system implementation.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[DEVFS]"       ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "fs/devfs.h"
#include "fs/vfs.h"
#include "libgen.h"
#include "stdio.h"
#include "string.h"
#include "sys/errno.h"
#include "time.h"
#include "system/syscall.h"
#include "sys/bitops.h"

/// Maximum length of name in DEVFS.
#define DEVFS_NAME_MAX 255U
/// Maximum number of files in DEVFS.
#define DEVFS_MAX_FILES 1024U
/// The magic number used to check if the devfs file is valid.
#define DEVFS_MAGIC_NUMBER 0xBF

// ============================================================================
// Data Structures
// ============================================================================

/// @brief Information concerning a file.
typedef struct devfs_file_t {
    /// Number used as delimiter, it must be set to 0xBF.
    int magic;
    /// The file inode.
    int inode;
    /// Flags.
    unsigned flags;
    /// The file mask.
    mode_t mask;
    /// The name of the file.
    char name[DEVFS_NAME_MAX];
    /// User id of the file.
    uid_t uid;
    /// Group id of the file.
    gid_t gid;
    /// Time of last access.
    time_t atime;
    /// Time of last data modification.
    time_t mtime;
    /// Time of last status change.
    time_t ctime;
    /// Pointer to the associated devfs_dir_entry_t.
    devfs_dir_entry_t dir_entry;
    /// Associated files.
    list_head files;
    /// List of devfs siblings.
    list_head siblings;
} devfs_file_t;

/// @brief The details regarding the filesystem.
/// @brief Contains the number of files inside the devfs filesystem.
typedef struct devfs_t {
    /// Number of files.
    unsigned int nfiles;
    /// List of headers.
    list_head files;
    /// Cache for creating new `devfs_file_t`.
    kmem_cache_t *devfs_file_cache;
} devfs_t;

/// The devfs filesystem.
static devfs_t fs;

// ============================================================================
// Forward Declaration of Functions
// ============================================================================

static vfs_file_t *devfs_open(const char *path, int flags, mode_t mode);
static int devfs_unlink(const char *path);
static int devfs_close(vfs_file_t *file);
static ssize_t devfs_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte);
static ssize_t devfs_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte);
static off_t devfs_lseek(vfs_file_t *file, off_t offset, int whence);
static int devfs_stat(const char *path, stat_t *stat);
static int devfs_fstat(vfs_file_t *file, stat_t *stat);
static int devfs_ioctl(vfs_file_t *file, int request, void *data);
static ssize_t devfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count);

// ============================================================================
// Virtual FileSystem (VFS) Operaions
// ============================================================================

/// Filesystem general operations.
static vfs_sys_operations_t devfs_sys_operations = {
    .mkdir_f   = NULL,
    .rmdir_f   = NULL,
    .stat_f    = devfs_stat,
    .creat_f   = NULL,
    .symlink_f = NULL,
};

/// Filesystem file operations.
static vfs_file_operations_t devfs_fs_operations = {
    .open_f     = devfs_open,
    .unlink_f   = devfs_unlink,
    .close_f    = devfs_close,
    .read_f     = devfs_read,
    .write_f    = devfs_write,
    .lseek_f    = devfs_lseek,
    .stat_f     = devfs_fstat,
    .ioctl_f    = devfs_ioctl,
    .getdents_f = devfs_getdents,
    .readlink_f = NULL,
};

// ============================================================================
// DEVFS Core Functions
// ============================================================================

/// @brief Checks if the file is a valid DEVFS file.
/// @param devfs_file the file to check.
/// @return true if valid, false otherwise.
static inline int devfs_check_file(devfs_file_t *devfs_file)
{
    return (devfs_file && (devfs_file->magic == DEVFS_MAGIC_NUMBER));
}

/// @brief Returns the DEVFS file associated with the given list entry.
/// @param entry the entry to transform to DEVFS file.
/// @return a valid pointer to a DEVFS file, NULL otherwise.
static inline devfs_file_t *devfs_get_file(list_head *entry)
{
    devfs_file_t *devfs_file;
    if (entry) {
        // Get the entry.
        devfs_file = list_entry(entry, devfs_file_t, siblings);
        // Check the file.
        if (devfs_file && devfs_check_file(devfs_file)) {
            return devfs_file;
        }
    }
    return NULL;
}

/// @brief Finds the DEVFS file at the given path.
/// @param path the path to the entry.
/// @return a pointer to the DEVFS file, NULL otherwise.
static inline devfs_file_t *devfs_find_entry_path(const char *path)
{
    devfs_file_t *devfs_file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            devfs_file = devfs_get_file(it);
            // Check its name.
            if (devfs_file && !strcmp(devfs_file->name, path)) {
                return devfs_file;
            }
        }
    }
    return NULL;
}

/// @brief Finds the DEVFS file with the given inode.
/// @param inode the inode we search.
/// @return a pointer to the DEVFS file, NULL otherwise.
static inline devfs_file_t *devfs_find_entry_inode(uint32_t inode)
{
    devfs_file_t *devfs_file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            devfs_file = devfs_get_file(it);
            // Check its inode.
            if (devfs_file && (devfs_file->inode == inode)) {
                return devfs_file;
            }
        }
    }
    return NULL;
}

/// @brief Finds the inode associated with a DEVFS file at the given path.
/// @param path the path to the entry.
/// @return a valid inode, or -1 on failure.
static inline int devfs_find_inode(const char *path)
{
    devfs_file_t *devfs_file = devfs_find_entry_path(path);
    if (devfs_file) {
        return devfs_file->inode;
    }
    return -1;
}

/// @brief Finds a free inode.
/// @return the free inode index, or -1 on failure.
static inline int devfs_get_free_inode(void)
{
    for (int inode = 1; inode < DEVFS_MAX_FILES; ++inode) {
        if (devfs_find_entry_inode(inode) == NULL) {
            return inode;
        }
    }
    return -1;
}

/// @brief Checks if the DEVFS directory at the given path is empty.
/// @param path the path to the directory.
/// @return 0 if empty, 1 if not.
static inline int devfs_check_if_empty(const char *path)
{
    devfs_file_t *devfs_file;
    if (!list_head_empty(&fs.files)) {
        char filedir[PATH_MAX];
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            devfs_file = devfs_get_file(it);
            // Check if it a valid pointer.
            if (devfs_file) {
                // It's the directory itself.
                if (!strcmp(path, devfs_file->name)) {
                    continue;
                }
                // Get the directory of the file.
                if (!dirname(devfs_file->name, filedir, sizeof(filedir))) {
                    continue;
                }
                // Check if directory path and file directory are the same.
                if (!strcmp(path, filedir)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/// @brief Creates a new DEVFS file.
/// @param path where the file resides.
/// @param flags the creation flags.
/// @return a pointer to the new DEVFS file, NULL otherwise.
static inline devfs_file_t *devfs_create_file(const char *path, unsigned flags)
{
    devfs_file_t *devfs_file = (devfs_file_t *)kmem_cache_alloc(fs.devfs_file_cache, GFP_KERNEL);
    if (!devfs_file) {
        pr_err("Failed to get free entry (%p).\n", devfs_file);
        return NULL;
    }
    // Clean up the memory.
    memset(devfs_file, 0, sizeof(devfs_file_t));
    // Initialize the magic number.
    devfs_file->magic = DEVFS_MAGIC_NUMBER;
    // Initialize the inode.
    devfs_file->inode = devfs_get_free_inode();
    // Flags.
    devfs_file->flags = flags;
    // The name of the file.
    strcpy(devfs_file->name, path);
    // Associated files.
    list_head_init(&devfs_file->files);
    // List of all the DEVFS files.
    list_head_init(&devfs_file->siblings);
    // Add the file to the list of opened files.
    list_head_insert_before(&devfs_file->siblings, &fs.files);
    // Time of last access.
    devfs_file->atime = sys_time(NULL);
    // Time of last data modification.
    devfs_file->mtime = devfs_file->atime;
    // Time of last status change.
    devfs_file->ctime = devfs_file->atime;
    // Initialize the dir_entry.
    devfs_file->dir_entry.name           = basename(devfs_file->name);
    devfs_file->dir_entry.data           = NULL;
    devfs_file->dir_entry.sys_operations = NULL;
    devfs_file->dir_entry.fs_operations  = NULL;
    // Increase the number of files.
    ++fs.nfiles;
    pr_debug("devfs_create_file(%p) `%s`\n", devfs_file, path);
    return devfs_file;
}

/// @brief Destroyes the given DEVFS file.
/// @param devfs_file pointer to the DEVFS file to destroy.
/// @return 0 on success, 1 on failure.
static inline int devfs_destroy_file(devfs_file_t *devfs_file)
{
    if (!devfs_file) {
        pr_err("Received a null entry (%p).\n", devfs_file);
        return 1;
    }
    pr_debug("devfs_destroy_file(%p) `%s`\n", devfs_file, devfs_file->name);
    // Remove the file from the list of opened files.
    list_head_remove(&devfs_file->siblings);
    // Free the cache.
    kmem_cache_free(devfs_file);
    // Decrease the number of files.
    --fs.nfiles;
    return 0;
}

/// @brief Creates a VFS file, from a DEVFS file.
/// @param devfs_file the DEVFS file.
/// @return a pointer to the newly create VFS file, NULL on failure.
static inline vfs_file_t *devfs_create_file_struct(devfs_file_t *devfs_file)
{
    if (!devfs_file) {
        pr_err("devfs_create_file_struct(%p): Devfs file not valid!\n", devfs_file);
        return NULL;
    }
    vfs_file_t *vfs_file = kmem_cache_alloc(vfs_file_cache, GFP_KERNEL);
    if (!vfs_file) {
        pr_err("devfs_create_file_struct(%p): Failed to allocate memory for VFS file!\n", devfs_file);
        return NULL;
    }
    memset(vfs_file, 0, sizeof(vfs_file_t));
    strcpy(vfs_file->name, devfs_file->name);
    vfs_file->device         = &devfs_file->dir_entry;
    vfs_file->ino            = devfs_file->inode;
    vfs_file->uid            = 0;
    vfs_file->gid            = 0;
    vfs_file->mask           = S_IRUSR | S_IRGRP | S_IROTH;
    vfs_file->length         = 0;
    vfs_file->flags          = devfs_file->flags;
    vfs_file->sys_operations = &devfs_sys_operations;
    vfs_file->fs_operations  = &devfs_fs_operations;
    list_head_init(&vfs_file->siblings);
    pr_debug("devfs_create_file_struct(%p): VFS file : %p\n", devfs_file, vfs_file);
    return vfs_file;
}

/// @brief Dumps on debugging output the DEVFS.
static void dump_devfs(void)
{
    devfs_file_t *file;
    if (!list_head_empty(&fs.files)) {
        list_for_each_decl(it, &fs.files)
        {
            // Get the file structure.
            file = devfs_get_file(it);
            // Check if it a valid devfs file.
            if (file) {
                pr_debug("[%3d] `%s`\n", file->inode, file->name);
            }
        }
    }
}

// ============================================================================
// Virtual FileSystem (VFS) Functions
// ============================================================================

/// @brief Open the file at the given path and returns its file descriptor.
/// @param path  The path to the file.
/// @param flags The flags used to determine the behavior of the function.
/// @param mode  The mode with which we open the file.
/// @return The file descriptor of the opened file, otherwise returns -1.
static vfs_file_t *devfs_open(const char *path, int flags, mode_t mode)
{
    char parent_path[PATH_MAX];
    if (!dirname(path, parent_path, sizeof(parent_path))) {
        return NULL;
    }
    // Get the parent path.
    // Check if the directories before it exist.
    if ((strcmp(parent_path, ".") != 0) && (strcmp(parent_path, "/") != 0)) {
        devfs_file_t *parent_file = devfs_find_entry_path(parent_path);
        if (parent_file == NULL) {
            pr_err("Cannot find parent `%s`.\n", parent_path);
            errno = ENOENT;
            return NULL;
        }
        if (!bitmask_check(parent_file->flags, DT_DIR)) {
            pr_err("Parent folder `%s` is not a directory.\n", parent_path);
            errno = ENOTDIR;
            return NULL;
        }
    }
    // Find the entry.
    devfs_file_t *devfs_file = devfs_find_entry_path(path);
    if (devfs_file != NULL) {
        // Check if the user wants to create a file.
        if (bitmask_check(flags, O_CREAT | O_EXCL)) {
            pr_err("Cannot create, it exists `%s`.\n", path);
            errno = EEXIST;
            return NULL;
        }
        // Check if the user wants to open a directory.
        if (bitmask_check(flags, O_DIRECTORY)) {
            // Check if the file is a directory.
            if (!bitmask_check(devfs_file->flags, DT_DIR)) {
                pr_err("Is not a directory `%s` but access requested involved a directory.\n", path);
                errno = ENOTDIR;
                return NULL;
            }
            // Check if pathname refers to a directory and the access requested
            // involved writing.
            if (bitmask_check(flags, O_RDWR) || bitmask_check(flags, O_WRONLY)) {
                pr_err("Is a directory `%s` but access requested involved writing.\n", path);
                errno = EISDIR;
                return NULL;
            }
            // Create the associated file.
            vfs_file_t *vfs_file = devfs_create_file_struct(devfs_file);
            if (!vfs_file) {
                pr_err("Cannot create vfs file for opening directory `%s`.\n", path);
                errno = ENFILE;
                return NULL;
            }
            // Update file access.
            devfs_file->atime = sys_time(NULL);
            // Add the vfs_file to the list of associated files.
            list_head_insert_before(&vfs_file->siblings, &devfs_file->files);
            return vfs_file;
        }
        // Create the associated file.
        vfs_file_t *vfs_file = devfs_create_file_struct(devfs_file);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Update file access.
        devfs_file->atime = sys_time(NULL);
        // Add the vfs_file to the list of associated files.
        list_head_insert_before(&vfs_file->siblings, &devfs_file->files);
        return vfs_file;
    }
    //  When both O_CREAT and O_DIRECTORY are specified in flags and the file
    //  specified by pathname does not exist, open() will create a regular file
    //  (i.e., O_DIRECTORY is ignored).
    if (bitmask_check(flags, O_CREAT)) {
        // Create the new devfs file.
        devfs_file = devfs_create_file(path, DT_REG);
        if (!devfs_file) {
            pr_err("Cannot create devfs_file for `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Create the associated file.
        vfs_file_t *vfs_file = devfs_create_file_struct(devfs_file);
        if (!vfs_file) {
            pr_err("Cannot create vfs file for opening file `%s`...\n", path);
            errno = ENFILE;
            return NULL;
        }
        // Add the vfs_file to the list of associated files.
        list_head_insert_before(&vfs_file->siblings, &devfs_file->files);
        pr_debug("Created file `%s`.\n", path);
        return vfs_file;
    }
    errno = ENOENT;
    return NULL;
}

/// @brief Closes the given file.
/// @param file The file structure.
/// @return 0 on success, -errno on failure.
static int devfs_close(vfs_file_t *file)
{
    assert(file && "Received null file.");
    //pr_debug("devfs_close(%p): VFS file : %p\n", file, file);
    // Remove the file from the list of `files` inside its corresponding `devfs_file_t`.
    list_head_remove(&file->siblings);
    // Free the memory of the file.
    kmem_cache_free(file);
    return 0;
}

/// @brief Deletes the file at the given path.
/// @param path The path to the file.
/// @return On success, zero is returned. On error, -1 is returned.
static inline int devfs_unlink(const char *path)
{
    if ((strcmp(path, ".") == 0) || (strcmp(path, "..") == 0)) {
        return -EPERM;
    }
    devfs_file_t *devfs_file = devfs_find_entry_path(path);
    if (devfs_file != NULL) {
        return -EEXIST;
    }
    // Check the type.
    if ((devfs_file->flags & DT_REG) == 0) {
        if ((devfs_file->flags & DT_DIR) != 0) {
            pr_err("devfs_unlink(%s): The file is a directory.\n", path);
            return -EISDIR;
        }
        pr_err("devfs_unlink(%s): The file is not a regular file.\n", path);
        return -EACCES;
    }
    // Check if the devfs file has still some file associated.
    if (!list_head_empty(&devfs_file->files)) {
        pr_err("devfs_unlink(%s): The file is opened by someone.\n", path);
        return -EACCES;
    }
    if (devfs_destroy_file(devfs_file)) {
        pr_err("devfs_unlink(%s): Failed to remove file.\n", path);
    }
    return 0;
}

/// @brief Reads from the file identified by the file descriptor.
/// @param file The file.
/// @param buffer Buffer where the read content must be placed.
/// @param offset Offset from which we start reading from the file.
/// @param nbyte The number of bytes to read.
/// @return The number of red bytes.
static ssize_t devfs_read(vfs_file_t *file, char *buffer, off_t offset, size_t nbyte)
{
    if (file) {
        devfs_file_t *devfs_file = devfs_find_entry_inode(file->ino);
        if (devfs_file && devfs_file->dir_entry.fs_operations) {
            if (devfs_file->dir_entry.fs_operations->read_f) {
                return devfs_file->dir_entry.fs_operations->read_f(file, buffer, offset, nbyte);
            }
        }
    }
    return -ENOSYS;
}

/// @brief Writes the given content inside the file.
/// @param file The file descriptor of the file.
/// @param buffer The content to write.
/// @param offset Offset from which we start writing in the file.
/// @param nbyte The number of bytes to write.
/// @return The number of written bytes.
static ssize_t devfs_write(vfs_file_t *file, const void *buffer, off_t offset, size_t nbyte)
{
    if (file) {
        devfs_file_t *devfs_file = devfs_find_entry_inode(file->ino);
        if (devfs_file && devfs_file->dir_entry.fs_operations) {
            if (devfs_file->dir_entry.fs_operations->write_f) {
                return devfs_file->dir_entry.fs_operations->write_f(file, buffer, offset, nbyte);
            }
        }
    }
    return -ENOSYS;
}

/// @brief Repositions the file offset inside a file.
/// @param file the file we are working with.
/// @param offset the offest to use for the operation.
/// @param whence the type of operation.
/// @return  Upon successful completion, returns the resulting offset
/// location as measured in bytes from the beginning of the file. On
/// error, the value (off_t) -1 is returned and errno is set to
/// indicate the error.
off_t devfs_lseek(vfs_file_t *file, off_t offset, int whence)
{
    if (file == NULL) {
        pr_err("Received a NULL file.\n");
        return -ENOSYS;
    }
    devfs_file_t *devfs_file = devfs_find_entry_inode(file->ino);
    if (devfs_file == NULL) {
        pr_err("There is no DEVFS fiel associated with the VFS file.\n");
        return -ENOSYS;
    }
    if (devfs_file->dir_entry.fs_operations) {
        if (devfs_file->dir_entry.fs_operations->lseek_f) {
            return devfs_file->dir_entry.fs_operations->lseek_f(file, offset, whence);
        }
    }
    return -EINVAL;
}

/// @brief Saves the information concerning the file.
/// @param file The file containing the data.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int __devfs_stat(devfs_file_t *file, stat_t *stat)
{
    if (!file) {
        pr_err("We received a NULL file pointer.\n");
        return -EFAULT;
    }
    if (!stat) {
        pr_err("We received a NULL stat pointer.\n");
        return -EFAULT;
    }
    if ((file->flags & DT_DIR)) {
        stat->st_mode = 0040000;
    } else if ((file->flags & DT_REG)) {
        stat->st_mode = 0100000;
    } else if ((file->flags & DT_LNK)) {
        stat->st_mode = 0120000;
    } else {
        return -ENOENT;
    }
    stat->st_mode  = stat->st_mode | file->mask;
    stat->st_uid   = file->uid;
    stat->st_gid   = file->gid;
    stat->st_dev   = 0;
    stat->st_ino   = file->inode;
    stat->st_size  = 0;
    stat->st_atime = file->atime;
    stat->st_mtime = file->mtime;
    stat->st_ctime = file->ctime;
    return 0;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param file The file struct.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int devfs_fstat(vfs_file_t *file, stat_t *stat)
{
    if (file && stat) {
        devfs_file_t *devfs_file = devfs_find_entry_inode(file->ino);
        if (devfs_file) {
            if (devfs_file->dir_entry.fs_operations && devfs_file->dir_entry.fs_operations->stat_f) {
                return devfs_file->dir_entry.fs_operations->stat_f(file, stat);
            }
            return __devfs_stat(devfs_file, stat);
        }
    }
    return -ENOSYS;
}

/// @brief Retrieves information concerning the file at the given position.
/// @param path The path to the file.
/// @param stat The structure where the information are stored.
/// @return 0 if success.
static int devfs_stat(const char *path, stat_t *stat)
{
    if (path && stat) {
        devfs_file_t *devfs_file = devfs_find_entry_path(path);
        if (devfs_file) {
            if (devfs_file->dir_entry.sys_operations && devfs_file->dir_entry.sys_operations->stat_f) {
                return devfs_file->dir_entry.sys_operations->stat_f(path, stat);
            }
            return __devfs_stat(devfs_file, stat);
        }
    }
    return -1;
}

/// @brief Perform the I/O control operation specified by REQUEST on FD. One
/// argument may follow; its presence and type depend on REQUEST.
/// @param file the file on which we perform the operations.
/// @param request the device-dependent request code
/// @param data an untyped pointer to memory.
/// @return Return value depends on REQUEST. Usually -1 indicates error.
static int devfs_ioctl(vfs_file_t *file, int request, void *data)
{
    if (file) {
        devfs_file_t *devfs_file = devfs_find_entry_inode(file->ino);
        if (devfs_file) {
            if (devfs_file->dir_entry.fs_operations && devfs_file->dir_entry.fs_operations->ioctl_f) {
                return devfs_file->dir_entry.fs_operations->ioctl_f(file, request, data);
            }
        }
    }
    return -1;
}

/// @brief Reads contents of the directories to a dirent buffer, updating
///        the offset and returning the number of written bytes in the buffer,
///        it assumes that all paths are well-formed.
/// @param file  The directory handler.
/// @param dirp  The buffer where the data should be written.
/// @param doff  The offset inside the buffer where the data should be written.
/// @param count The maximum length of the buffer.
/// @return The number of written bytes in the buffer.
static inline ssize_t devfs_getdents(vfs_file_t *file, dirent_t *dirp, off_t doff, size_t count)
{
    if (!file || !dirp) {
        return -1;
    }
    // Check if the size of the buffer is big enough to hold the data about the
    // directory entry.
    if (count < sizeof(dirent_t)) {
        return -1;
    }
    // If there are no file, stop right here.
    if (list_head_empty(&fs.files)) {
        return 0;
    }
    // Find the directory entry.
    devfs_file_t *direntry = devfs_find_entry_inode(file->ino);
    if (direntry == NULL) {
        return -ENOENT;
    }
    // Check if it is a directory.
    if ((direntry->flags & DT_DIR) == 0) {
        return -ENOTDIR;
    }
    // Clear the buffer.
    memset(dirp, 0, count);
    // Initialize, the length of the directory name.
    size_t len           = strlen(direntry->name);
    ssize_t written_size = 0;
    off_t iterated_size  = 0;
    char parent_path[PATH_MAX];
    // Iterate the filesystem files.
    list_for_each_decl(it, &fs.files)
    {
        // Get the file structure.
        devfs_file_t *entry = devfs_get_file(it);
        // Check if it a valid devfs file.
        if (!entry) {
            continue;
        }
        // If the entry is the directory itself, skip.
        if (!strcmp(direntry->name, entry->name)) {
            continue;
        }
        // Get the parent directory.
        if (!dirname(entry->name, parent_path, sizeof(parent_path))) {
            continue;
        }
        // Check if the parent of the entry is the directory we are iterating.
        if (strcmp(direntry->name, parent_path) != 0) {
            continue;
        }
        // Advance the size we just iterated.
        iterated_size += sizeof(dirent_t);
        // Check if the iterated size is still below the offset.
        if (iterated_size <= doff) {
            continue;
        }
        // Check if the last character of the entry is a slash.
        if (*(entry->name + len) == '/') {
            ++len;
        }
        // Write on current dirp.
        dirp->d_ino  = entry->inode;
        dirp->d_type = entry->flags;
        strcpy(dirp->d_name, entry->name + len);
        dirp->d_off    = sizeof(dirent_t);
        dirp->d_reclen = sizeof(dirent_t);
        // Increment the written counter.
        written_size += sizeof(dirent_t);
        // Move to next writing position.
        ++dirp;
        if (written_size >= count) {
            break;
        }
    }
    return written_size;
}

/// @brief Mounts the block device as a devfs filesystem.
/// @param block_device the block device formatted as devfs.
/// @param path location where we mount the filesystem.
/// @return the VFS root node of the devfs filesystem.
static vfs_file_t *devfs_mount(vfs_file_t *block_device, const char *path)
{
    return NULL;
}

// ============================================================================
// Initialization Functions
// ============================================================================

/// @brief Mounts the filesystem at the given path.
/// @param path the path where we want to mount a devfs.
/// @param device we expect it to be NULL.
/// @return a pointer to the root VFS file.
static vfs_file_t *devfs_mount_callback(const char *path, const char *device)
{
    pr_debug("devfs_mount_callback(%s, %s)\n", path, device);
    // Create the new devfs file.
    devfs_file_t *root = devfs_create_file(path, DT_DIR);
    if (!root) {
        pr_err("Cannot create mount point `%s` for device `%s`.\n", path, device);
        return NULL;
    }
    // Set the mask.
    root->mask = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
    // Create the associated file.
    vfs_file_t *file = devfs_create_file_struct(root);
    if (!file) {
        pr_err("Cannot create VFS file for `%s` and device `%s`.\n", path, device);
        return NULL;
    }
    // Add the file to the list of associated files.
    list_head_insert_before(&file->siblings, &root->files);
    // Initialize the root.
    return file;
}

/// Filesystem information.
static file_system_type devfs_file_system_type = {
    .name     = "devfs",
    .fs_flags = 0,
    .mount    = devfs_mount_callback
};

int devfs_module_init(void)
{
    // Initialize the devfs.
    memset(&fs, 0, sizeof(struct devfs_t));
    // Initialize the cache.
    fs.devfs_file_cache = KMEM_CREATE(devfs_file_t);
    // Initialize the list of devfs files.
    list_head_init(&fs.files);
    // Register the filesystem.
    vfs_register_filesystem(&devfs_file_system_type);
    return 0;
}

int devfs_cleanup_module(void)
{
    // Destroy the cache.
    kmem_cache_destroy(fs.devfs_file_cache);
    // Unregister the filesystem.
    vfs_unregister_filesystem(&devfs_file_system_type);
    return 0;
}

// ============================================================================
// Publically available functions
// ============================================================================

devfs_dir_entry_t *devfs_dir_entry_get(const char *name)
{
    // Get the devfs entry.
    devfs_file_t *devfs_file = devfs_find_entry_path(name);
    if (devfs_file == NULL) {
        pr_err("devfs_dir_entry_get(%s): Cannot find devfs entry.\n", name);
        return NULL;
    }
    return &devfs_file->dir_entry;
}

devfs_dir_entry_t *devfs_create_entry(const char *name)
{
    // Check if the entry exists.
    if (devfs_find_entry_path(name) != NULL) {
        pr_err("devfs_create_entry(%s): Devfs entry already exists.\n", name);
        errno = EEXIST;
        return NULL;
    }
    // Create the new devfs file.
    devfs_file_t *devfs_file = devfs_create_file(name, DT_REG);
    if (!devfs_file) {
        pr_err("devfs_create_entry(%s): Cannot create devfs entry.\n", name);
        errno = ENFILE;
        return NULL;
    }
    return &devfs_file->dir_entry;
}

int devfs_destroy_entry(const char *name)
{
    // Check if the entry exists.
    devfs_file_t *devfs_file = devfs_find_entry_path(name);
    if (devfs_file == NULL) {
        pr_err("devfs_destroy_entry(%s): Cannot find devfs entry.\n", name);
        return -ENOENT;
    }
    // Check if the devfs file has still some file associated.
    if (!list_head_empty(&devfs_file->files)) {
        pr_err("devfs_destroy_entry(%s): Devfs entry is busy.\n", name);
        return -EBUSY;
    }
    if (devfs_destroy_file(devfs_file)) {
        pr_err("devfs_destroy_entry(%s): Failed to remove file.\n", name);
        return -ENOENT;
    }
    return 0;
}

int devfs_entry_set_mask(devfs_dir_entry_t *entry, mode_t mask)
{
    devfs_file_t *devfs_file = container_of(entry, devfs_file_t, dir_entry);
    if (devfs_file == NULL) {
        pr_err("devfs_destroy_entry(%s): Cannot find devfs entry.\n", entry->name);
        return -ENOENT;
    }
    devfs_file->mask = mask;
    return 0;
}

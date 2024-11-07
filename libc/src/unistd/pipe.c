/// @file   pipe.c
/// @brief  System call wrapper for creating a pipe
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

// Define the `pipe` system call wrapper using a macro similar to `_syscall1`

int pipe(int fds[2])
{
    long __res;
    __inline_syscall1(__res, pipe, fds);
    __syscall_return(int, __res);
}

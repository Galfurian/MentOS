/// @file setgid.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "errno.h"
#include "system/syscall_types.h"

_syscall1(int, setgid, pid_t, pid)
_syscall2(int, setregid, gid_t, rgid, gid_t, egid)

/// @file dup.c
/// @brief
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "unistd.h"
#include "system/syscall_types.h"
#include "errno.h"

_syscall1(int, dup, int, fd)

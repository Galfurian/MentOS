/// @file readline.h
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "stddef.h"

/// @brief Reads a line from the file.
/// @param fd the file descriptor.
/// @param buffer the buffer where we place the line.
/// @param buflen the length of the buffer.
/// @param read_len pointer to a variable where the length of the read line will
/// be stored. If NULL, the length will not be stored.
/// @return  1 if a newline character ('\n') was found,
///         -1 if the line was terminated by EOF or null character without finding a newline,
///          0 if no data was read (indicating end of file or error).
int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len);

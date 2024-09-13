/// @file readline.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Contains the readline function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "readline.h"

#include "stdio.h"
#include "string.h"
#include "sys/unistd.h"

int readline(int fd, char *buffer, size_t buflen, ssize_t *read_len)
{
    ssize_t length, rollback, num_read;
    unsigned char found_newline = 1;
    char *newline;

    // Clear the buffer to avoid junk data.
    memset(buffer, 0, buflen);

    // Read data from the file descriptor into the buffer.
    num_read = read(fd, buffer, buflen);
    if (num_read == 0) {
        // If no data is read, return 0 (likely end of file).
        return 0;
    }

    // Search for a newline character in the buffer.
    if ((newline = strchr(buffer, '\n')) == NULL) {
        found_newline = 0;
        // Search for a EOF character in the buffer.
        if ((newline = strchr(buffer, EOF)) == NULL) {
            // Search for a null terminator character in the buffer.
            if ((newline = strchr(buffer, 0)) == NULL) {
                return 0;
            }
        }
    }

    // Calculate the length of the line based on the position of the newline (or
    // other terminator).
    length = (newline - buffer);
    if (length <= 0) {
        // If the length is invalid (zero or negative), return 0.
        return 0;
    }

    // Null-terminate the string in the buffer at the position of the newline.
    buffer[length] = 0;

    // Compute how much we need to roll back the file position to exclude the
    // newline.
    rollback = length - num_read + 1;
    if (rollback > 1) {
        // If rollback is greater than 1, it indicates an invalid read, so
        // return 0.
        return 0;
    }

    // Roll back the file descriptor's read position to the character after the
    // newline.
    lseek(fd, rollback, SEEK_CUR);

    // If the caller provided a variable for the read length, store the length
    // of the line.
    if (read_len) {
        *read_len = length;
    }

    // Return 1 if a newline was found, or -1 if terminated by EOF or null
    // character.
    return (found_newline) ? 1 : -1;
}

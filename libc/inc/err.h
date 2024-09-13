/// @file err.h
/// @brief Contains err functions
/// @copyright (c) 2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include <stdarg.h>

/// @brief Print formatted error message on stderr and exit.
/// @param eval the exit value.
/// @param fmt the format string.
void err(int eval, const char *fmt, ...);

/// @brief Print formatted error message on stderr and exit.
/// @param eval the exit value.
/// @param fmt the format string.
/// @param args the arguments to use to fomar the string.
void verr(int eval, const char *fmt, va_list args);

/// @brief Print formatted message on stderr without appending an error message and exit
/// @param eval the exit value.
/// @param fmt the format string.
void errx(int eval, const char *fmt, ...);

/// @brief Print formatted message on stderr without appending an error message and exit
/// @param eval the exit value.
/// @param fmt the format string.
/// @param args the arguments to use to fomar the string.
void verrx(int eval, const char *fmt, va_list args);

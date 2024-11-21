# External functions
.extern main
.extern __libc_start_main

# Global entry point
.global _start

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

_start:                     # _start is the entry point known to the linker
    movl $0, %ebp           # Set ebp to 0 as x86 programs require
    pushl $main             # Push the pointer to `main` onto the stack
    call __libc_start_main  # Call the libc initialization function
    movl %eax, %ebx         # Move `main` return value to ebx
    movl $1, %eax           # Exit system call number
    int $0x80               # Perform system call

#                MentOS, The Mentoring Operating system project
# @file   boot.s
# @brief  Kernel start location, multiboot header
#         GRUB-compatible multiboot header for kernel loading.
#         Includes stack setup and handoff to `boot_main`.
# Copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

# =============================================================================
# SECTION (multiboot_header)
# =============================================================================
.section .multiboot_header, "a"
.align 4
.global multiboot_header
multiboot_header:
    .long 0x1BADB002                  # Magic number
    .long 0x00000003                  # Flags
    .long -(0x1BADB002 + 0x00000003)  # Checksum

# =============================================================================
# SECTION (text)
# =============================================================================
.section .text
.global boot_entry
boot_entry:
    # Clear interrupt flag [IF = 0]
    cli
    # Set up the stack pointer
    movl $stack_top, %esp
    # Pass initial ESP
    pushl %esp
    # Pass Multiboot info structure
    pushl %ebx
    # Pass Multiboot magic number
    pushl %eax
    # Call the boot_main() function inside boot.c
    call boot_main
    # Clear interrupts and hang if we return from boot_main
    cli
hang:
    hlt
    jmp hang

.global boot_kernel
boot_kernel:
    movl 4(%esp), %edx  # Stack pointer
    movl 8(%esp), %ebx  # Entry
    movl 12(%esp), %eax # Boot info
    movl %edx, %esp     # Set stack pointer
    pushl %eax          # Push the boot info
    call *%ebx          # Call the kernel main

# =============================================================================
# SECTION (bss)
# =============================================================================
.section .bss
.align 16

.global stack_bottom
stack_bottom:
    .skip 0x100000

.global stack_top
stack_top:
    # The top of the stack is the bottom because the stack counts down.

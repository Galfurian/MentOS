#                MentOS, The Mentoring Operating system project
# @file   exception.s
# @brief
# @copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

.extern isr_handler

# Macro used to define an ISR which does not push an error code.
.macro ISR_NOERR num
.global INT_\num
INT_\num:
    cli
    # A normal ISR stub that pops a dummy error code to keep a
    # uniform stack frame
    pushl $0
    pushl $\num
    jmp isr_common
.endm

# Macro used to define an ISR which pushes an error code.
.macro ISR_ERR num
.global INT_\num
INT_\num:
    cli
    pushl $\num
    jmp isr_common
.endm

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

# Standard x86 interrupt service routines
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

ISR_NOERR 80

isr_common:
    # Save all registers (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    pusha

    # Save segment registers
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs

    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs

    # CLD - Clear Direction Flag
    cld

    # Call the interrupt handler.
    pushl %esp
    call isr_handler
    addl $0x4, %esp

    # Restore segment registers.
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # Restore all registers (eax, ecx, edx, ebx, esp, ebp, esi, edi).
    popa

    # Cleanup error code and IRQ #
    addl $0x8, %esp

    iret                        # Pops 5 things at once:
                                #   CS, EIP, EFLAGS, SS, and ESP

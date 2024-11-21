#                MentOS, The Mentoring Operating system project
# @file   interrupt.s
# @brief
# @copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

.extern irq_handler

# Define a macro to create IRQ handlers.
.macro IRQ irq_num, irq_vector
.global IRQ_\irq_num
IRQ_\irq_num:
    cli                       # Disable interrupt line
    # A normal ISR stub that pops a dummy error code to keep a
    # uniform stack frame
    pushl $0                  # Push dummy error code
    pushl $\irq_vector        # Push IRQ number
    jmp irq_common            # Jump to common IRQ handler
.endm

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

# 32 is the first IRQ, 47 is the last one. DO NOT CHANGE THESE NUMBERS.
IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

irq_common:
    #==== Save CPU registers ===================================================
    # When an IRQ occurs, the following registers are already pushed on stack:
    # EIP, CS, EFLAGS, USERESP, SS

    # Save general-purpose registers
    pusha

    # Save segment registers
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    #----------------------------------------------------------------------------

    #==== Ensure we are using kernel data segment ===============================
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    cld
    #----------------------------------------------------------------------------

    #==== Call the interrupt handler ============================================
    # The argument passed is a pointer to an Interrupt_State struct,
    # which describes the stack layout for all interrupts.
    pushl %esp
    call irq_handler
    addl $4, %esp              # Remove ESP from stack
    #----------------------------------------------------------------------------

    #==== Restore registers =====================================================
    # Restore segment registers
    popl %gs
    popl %fs
    popl %es
    popl %ds

    # Restore general-purpose registers
    popa
    #----------------------------------------------------------------------------

    # Cleanup error code and IRQ #
    addl $8, %esp

    # Return to process
    iret                        # Pops 5 things at once:
                                #   CS, EIP, EFLAGS, SS, and ESP

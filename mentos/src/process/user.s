#                MentOS, The Mentoring Operating system project
# @file   user.s
# @brief
# Enter userspace (Ring 3) from Ring 0 (Kernel mode).
# Usage: enter_userspace(uintptr_t location, uintptr_t stack)
# Copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

.global enter_userspace

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

enter_userspace:
    pushl %ebp            # Save current EBP
    movl %esp, %ebp       # Open a new stack frame

    #==== Segment Selector =====================================================
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    # We don't need to worry about SS; it's handled by `iret`.
    #----------------------------------------------------------------------------

    #==== Prepare the Stack for `iret` =========================================
    # Required stack structure before `iret`:
    #    SS           --> Segment selector
    #    ESP          --> Stack address
    #    EFLAGS       --> CPU state flags
    #    CS           --> Code segment
    #    EIP          --> Entry point

    #==== User Data Segment with Bottom 2 Bits Set for Ring 3 ==================
    pushl $0x23            # Push SS onto the Kernel's stack
    #----------------------------------------------------------------------------

    #==== ESP: Stack Address ===================================================
    movl 8(%ebp), %eax     # ARG1: uintptr_t stack (offset 0x08 from EBP)
    pushl %eax             # Push process's stack address onto Kernel's stack
    #----------------------------------------------------------------------------

    #==== EFLAGS: CPU State Flags ==============================================
    pushf                  # Push EFLAGS onto the Kernel's stack
    popl %eax              # Pop EFLAGS into EAX
    orl $0x200, %eax       # Enable interrupts (set IF bit in EFLAGS)
    pushl %eax             # Push updated EFLAGS onto the Kernel's stack
    #----------------------------------------------------------------------------

    #==== CS: Code Segment =====================================================
    pushl $0x1b            # Push Code Segment (Ring 3 CS selector) onto the stack
    #----------------------------------------------------------------------------

    #==== EIP: Entry Point =====================================================
    movl 4(%ebp), %eax     # ARG0: uintptr_t location (offset 0x04 from EBP)
    pushl %eax             # Push uintptr_t location onto the Kernel's stack
    #----------------------------------------------------------------------------

    iret                   # Interrupt return: switch to Ring 3 (userspace)
    popl %ebp              # Restore EBP (if we somehow get here, which we shouldn't)
    ret                    # Return (should never reach here)
    # We should NOT still be here!

#                MentOS, The Mentoring Operating system project
# @file   gdt.s
# @brief
# @copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

.global gdt_flush        # Allows the C code to call gdt_flush().

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

gdt_flush:
    movl 4(%esp), %eax    # Get the pointer to the GDT, passed as a parameter.
    lgdt (%eax)           # Load the new GDT pointer.

    # The data segments selectors (registers) can be modified using
    # simple mov instructions, but cs can't be directly set with mov, so we use:
    ljmp $0x08, $flush    # to load the segment configurations into the code selector.

flush:
    movw $0x10, %ax       # 0x10 is the offset in the GDT to our data segment.
    movw %ax, %ds         # Load all data segment selectors.
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    ret
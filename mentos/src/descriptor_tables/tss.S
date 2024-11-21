#                MentOS, The Mentoring Operating system project
# @file   tss.s
# @brief
# @copyright (c) 2014-2021 This file is distributed under the MIT License.
# See LICENSE.md for details.

.global tss_flush

# -----------------------------------------------------------------------------
# SECTION (text)
# -----------------------------------------------------------------------------
.section .text

tss_flush:
    movw $0x28, %ax   # Load 0x28 into AX
    ltr %ax           # Load the task register
    ret               # Return

/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * ARM64 AAPCS context switching for Mach-O (macOS, iOS)
 *
 * Based on boost::context by Edward Nevill + Oliver Kowalke (Boost Software License)
 */

/*******************************************************
 *                                                     *
 *  Context data layout (176 bytes = 0xb0):            *
 *  -------------------------------------------------  *
 *  | 0x00 - 0x3f |  d8-d15 (FP callee-saved)        |  *
 *  -------------------------------------------------  *
 *  | 0x40 - 0x8f |  x19-x28 (GP callee-saved)       |  *
 *  -------------------------------------------------  *
 *  | 0x90        |  FP (x29), LR (x30)              |  *
 *  -------------------------------------------------  *
 *  | 0xa0        |  PC (return address)             |  *
 *  | 0xa8        |  alignment padding               |  *
 *  -------------------------------------------------  *
 *                                                     *
 *******************************************************/

.text

/*
 * fcontext_t sh_make_fcontext(void* sp, void* stack_bottom, void (*fn)(transfer_t))
 *
 * Arguments:
 *   x0 - stack pointer (top of allocated stack)
 *   x1 - stack bottom (unused on this platform)
 *   x2 - pointer to context function
 * Returns:
 *   x0 - new context handle
 */
.private_extern _sh_make_fcontext
.globl _sh_make_fcontext
.balign 16
_sh_make_fcontext:
    ; align stack pointer to 16-byte boundary
    and x0, x0, ~0xF

    ; reserve space for context-data (0xb0 = 176 bytes)
    sub x0, x0, #0xb0

    ; store context-function address as PC
    str x2, [x0, #0xa0]

    ; compute address of finish and store as LR
    adr x1, L_sh_finish
    str x1, [x0, #0x98]

    ; return pointer to context-data
    ret lr

L_sh_finish:
    ; context-function returned - exit with code 0
    mov x0, #0
    bl __exit

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * Arguments:
 *   x0 - target context to switch to
 *   x1 - user data to pass to target
 * Returns:
 *   transfer_t { x0 = fctx, x1 = data }
 */
.private_extern _sh_jump_fcontext
.globl _sh_jump_fcontext
.balign 16
_sh_jump_fcontext:
    ; reserve stack for context-data
    sub sp, sp, #0xb0

    ; save FP registers d8-d15
    stp d8,  d9,  [sp, #0x00]
    stp d10, d11, [sp, #0x10]
    stp d12, d13, [sp, #0x20]
    stp d14, d15, [sp, #0x30]

    ; save GP registers x19-x28
    stp x19, x20, [sp, #0x40]
    stp x21, x22, [sp, #0x50]
    stp x23, x24, [sp, #0x60]
    stp x25, x26, [sp, #0x70]
    stp x27, x28, [sp, #0x80]
    stp fp,  lr,  [sp, #0x90]

    ; save LR as PC (return address)
    str lr, [sp, #0xa0]

    ; store current SP in x4 (will become return value)
    mov x4, sp

    ; switch to target stack
    mov sp, x0

    ; restore FP registers d8-d15
    ldp d8,  d9,  [sp, #0x00]
    ldp d10, d11, [sp, #0x10]
    ldp d12, d13, [sp, #0x20]
    ldp d14, d15, [sp, #0x30]

    ; restore GP registers x19-x28
    ldp x19, x20, [sp, #0x40]
    ldp x21, x22, [sp, #0x50]
    ldp x23, x24, [sp, #0x60]
    ldp x25, x26, [sp, #0x70]
    ldp x27, x28, [sp, #0x80]
    ldp fp,  lr,  [sp, #0x90]

    ; prepare return value: transfer_t { x0 = fctx, x1 = data }
    ; x1 already contains data, move saved SP to x0
    mov x0, x4

    ; load PC (return address)
    ldr x4, [sp, #0xa0]

    ; deallocate context-data from stack
    add sp, sp, #0xb0

    ; jump to target
    ret x4

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 *
 * Arguments:
 *   x0 - target context
 *   x1 - user data
 *   x2 - ontop function
 * Returns:
 *   transfer_t from ontop function
 */
.private_extern _sh_ontop_fcontext
.globl _sh_ontop_fcontext
.balign 16
_sh_ontop_fcontext:
    ; save ontop function pointer
    mov x3, x2

    ; reserve stack for context-data
    sub sp, sp, #0xb0

    ; save FP registers d8-d15
    stp d8,  d9,  [sp, #0x00]
    stp d10, d11, [sp, #0x10]
    stp d12, d13, [sp, #0x20]
    stp d14, d15, [sp, #0x30]

    ; save GP registers x19-x28
    stp x19, x20, [sp, #0x40]
    stp x21, x22, [sp, #0x50]
    stp x23, x24, [sp, #0x60]
    stp x25, x26, [sp, #0x70]
    stp x27, x28, [sp, #0x80]
    stp fp,  lr,  [sp, #0x90]

    ; save LR as PC
    str lr, [sp, #0xa0]

    ; store current SP in x4
    mov x4, sp

    ; switch to target stack
    mov sp, x0

    ; restore FP registers d8-d15
    ldp d8,  d9,  [sp, #0x00]
    ldp d10, d11, [sp, #0x10]
    ldp d12, d13, [sp, #0x20]
    ldp d14, d15, [sp, #0x30]

    ; restore GP registers x19-x28
    ldp x19, x20, [sp, #0x40]
    ldp x21, x22, [sp, #0x50]
    ldp x23, x24, [sp, #0x60]
    ldp x25, x26, [sp, #0x70]
    ldp x27, x28, [sp, #0x80]
    ldp fp,  lr,  [sp, #0x90]

    ; prepare transfer_t argument: { x0 = fctx, x1 = data }
    mov x0, x4

    ; deallocate context-data
    add sp, sp, #0xb0

    ; jump to ontop function (x3)
    br x3

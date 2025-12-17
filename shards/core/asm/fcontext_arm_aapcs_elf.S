/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * ARM (32-bit) AAPCS context switching for ELF (Linux, Android)
 *
 * Based on boost::context by Oliver Kowalke (Boost Software License)
 */

/*******************************************************
 *                                                     *
 *  Context data layout (128 bytes):                   *
 *  -------------------------------------------------  *
 *  | 0x00-0x3c | s16-s31 (VFP callee-saved)         |  *
 *  -------------------------------------------------  *
 *  | 0x40      | hidden (struct return ptr)         |  *
 *  | 0x44-0x5c | v1-v7 (r4-r10)                     |  *
 *  -------------------------------------------------  *
 *  | 0x60      | v8 (r11/fp)                        |  *
 *  | 0x64      | lr (r14)                           |  *
 *  | 0x68      | pc (entry point)                   |  *
 *  | 0x6c-0x74 | FCTX, DATA (transfer_t return)     |  *
 *  -------------------------------------------------  *
 *                                                     *
 *  AAPCS register names:                              *
 *  a1-a4 = r0-r3 (argument/scratch)                   *
 *  v1-v8 = r4-r11 (callee-saved)                      *
 *  ip = r12 (scratch)                                 *
 *  sp = r13, lr = r14, pc = r15                       *
 *                                                     *
 *******************************************************/

.file "fcontext_arm_aapcs_elf.S"
.text
.syntax unified

/*
 * fcontext_t sh_make_fcontext(void* sp, size_t size, void (*fn)(transfer_t))
 *
 * Arguments:
 *   a1 (r0) - stack pointer (top of allocated stack)
 *   a2 (r1) - stack size (unused)
 *   a3 (r2) - pointer to context function
 * Returns:
 *   a1 (r0) - new context handle
 */
.globl sh_make_fcontext
.type sh_make_fcontext,%function
.align 2
sh_make_fcontext:
    @ Align stack pointer to 16-byte boundary
    bic a1, a1, #15

    @ Reserve space for context-data (0x7c = 124 bytes)
    sub a1, a1, #124

    @ Store context-function address as PC
    str a3, [a1, #104]

    @ Compute address for transfer_t return value
    add a2, a1, #108
    str a2, [a1, #64]

    @ Compute address of finish and store as LR
    adr a2, .L_sh_finish
    str a2, [a1, #100]

    @ Return pointer to context-data
    bx lr

.L_sh_finish:
    @ Context-function returned - exit with code 0
    mov a1, #0
    bl _exit@PLT

.size sh_make_fcontext,.-sh_make_fcontext

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * Arguments:
 *   a1 (r0) - hidden pointer for struct return (implicit)
 *   a2 (r1) - target context to switch to
 *   a3 (r2) - user data to pass to target
 * Returns:
 *   transfer_t via hidden pointer
 *
 * Note: ARM AAPCS returns structs >4 bytes via hidden first argument
 */
.globl sh_jump_fcontext
.type sh_jump_fcontext,%function
.align 2
sh_jump_fcontext:
    @ Save PC (LR) on stack first
    push {lr}
    @ Save hidden pointer, v1-v8, lr
    push {a1, v1-v8, lr}

    @ Reserve space for VFP registers
    sub sp, sp, #64
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    @ Save VFP callee-saved registers s16-s31 (d8-d15)
    vstmia sp, {d8-d15}
#endif

    @ Store current SP in a1 (will become return fctx)
    mov a1, sp

    @ Switch to target stack (a2 = target context)
    mov sp, a2

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    @ Restore VFP registers
    vldmia sp, {d8-d15}
#endif
    @ Deallocate VFP space
    add sp, sp, #64

    @ Restore hidden pointer, v1-v8, lr
    pop {a4, v1-v8, lr}

    @ Write transfer_t to hidden pointer location
    @ a4 = hidden pointer, a1 = source context, a3 = data
    str a1, [a4, #0]
    str a3, [a4, #4]

    @ Set up arguments for context function
    @ a1 = fctx (already set), a2 = data
    mov a2, a3

    @ Restore PC and jump to target
    pop {pc}

.size sh_jump_fcontext,.-sh_jump_fcontext

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 *
 * Arguments:
 *   a1 (r0) - hidden pointer
 *   a2 (r1) - target context
 *   a3 (r2) - user data
 *   a4 (r3) - ontop function
 */
.globl sh_ontop_fcontext
.type sh_ontop_fcontext,%function
.align 2
sh_ontop_fcontext:
    @ Save ontop function in ip (r12)
    mov ip, a4

    @ Save PC (LR) on stack first
    push {lr}
    @ Save hidden pointer, v1-v8, lr
    push {a1, v1-v8, lr}

    @ Reserve space for VFP registers
    sub sp, sp, #64
#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    @ Save VFP registers
    vstmia sp, {d8-d15}
#endif

    @ Store current SP in a1
    mov a1, sp

    @ Switch to target stack
    mov sp, a2

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))
    @ Restore VFP registers
    vldmia sp, {d8-d15}
#endif
    add sp, sp, #64

    @ Restore hidden pointer, v1-v8, lr
    pop {a4, v1-v8, lr}

    @ Write transfer_t to hidden pointer
    str a1, [a4, #0]
    str a3, [a4, #4]

    @ Set up arguments
    mov a2, a3

    @ Deallocate PC slot
    add sp, sp, #4

    @ Jump to ontop function
    bx ip

.size sh_ontop_fcontext,.-sh_ontop_fcontext

@ Mark that we don't need executable stack
.section .note.GNU-stack,"",%progbits

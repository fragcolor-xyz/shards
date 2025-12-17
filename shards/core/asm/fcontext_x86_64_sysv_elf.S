/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * x86_64 System V ABI context switching for ELF (Linux, BSD)
 *
 * Based on boost::context by Oliver Kowalke (Boost Software License)
 * Note: Does not include TLS stack guard or CET shadow stack support
 */

/****************************************************************************************
 *                                                                                      *
 *  Context data layout on stack (64 bytes = 0x40):                                     *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x00  |   0x04  |   0x08  |   0x0c  |   0x10   |  0x14   |  0x18   |  0x1c   |  *
 *  ----------------------------------------------------------------------------------  *
 *  | fc_mxcsr|fc_x87_cw|        R12        |         R13        |        R14        |  *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x20  |   0x24  |   0x28  |   0x2c  |   0x30   |  0x34   |  0x38   |  0x3c   |  *
 *  ----------------------------------------------------------------------------------  *
 *  |        R15        |        RBX        |         RBP        |        RIP        |  *
 *  ----------------------------------------------------------------------------------  *
 *                                                                                      *
 ****************************************************************************************/

#ifdef __i386__
# include "fcontext_x86_sysv_elf.S"
#else

.file "fcontext_x86_64_sysv_elf.S"
.text

/*
 * fcontext_t sh_make_fcontext(void* sp, void* stack_bottom, void (*fn)(transfer_t))
 *
 * Creates a new context on the given stack.
 * Arguments:
 *   rdi - stack pointer (top of allocated stack)
 *   rsi - stack bottom (unused on this platform)
 *   rdx - pointer to context function
 * Returns:
 *   rax - new context handle (pointer to context data on stack)
 */
.globl sh_make_fcontext
.type sh_make_fcontext,@function
.align 16
sh_make_fcontext:
    /* first arg == top of context-stack */
    movq  %rdi, %rax

    /* align stack pointer to 16-byte boundary */
    andq  $-16, %rax

    /* reserve space for context-data on stack */
    /* 0x40 bytes for context + 0x8 for alignment = 0x48 */
    /* on context-function entry: (RSP - 0x8) % 16 == 0 */
    leaq  -0x48(%rax), %rax

    /* save address of context-function in RBX slot */
    movq  %rdx, 0x28(%rax)

    /* save MMX control and status word */
    stmxcsr  (%rax)
    /* save x87 control word */
    fnstcw   0x4(%rax)

    /* compute address of trampoline */
    leaq  .L_sh_trampoline(%rip), %rcx
    /* save trampoline as return address (called after first jump) */
    movq  %rcx, 0x38(%rax)

    /* compute address of finish routine */
    leaq  .L_sh_finish(%rip), %rcx
    /* save finish as RBP (will be pushed by trampoline) */
    movq  %rcx, 0x30(%rax)

    /* return pointer to context-data */
    ret

.L_sh_trampoline:
    /* push finish address to stack (for return from context function) */
    push %rbp
    /* jump to context-function (address in RBX) */
    jmp *%rbx

.L_sh_finish:
    /* context-function returned - exit with code 0 */
    xorq  %rdi, %rdi
    call  _exit@PLT
    hlt

.size sh_make_fcontext,.-sh_make_fcontext

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * Switches to another context.
 * Arguments:
 *   rdi - target context to switch to
 *   rsi - user data to pass to target
 * Returns:
 *   transfer_t { fctx = source context, data = passed data }
 *   (rax = fctx, rdx = data)
 */
.globl sh_jump_fcontext
.type sh_jump_fcontext,@function
.align 16
sh_jump_fcontext:
    /* reserve space for context-data on current stack */
    leaq  -0x40(%rsp), %rsp

    /* save FPU state */
    stmxcsr  (%rsp)
    fnstcw   0x4(%rsp)

    /* save callee-saved registers */
    movq  %r12, 0x8(%rsp)
    movq  %r13, 0x10(%rsp)
    movq  %r14, 0x18(%rsp)
    movq  %r15, 0x20(%rsp)
    movq  %rbx, 0x28(%rsp)
    movq  %rbp, 0x30(%rsp)

    /* save return address at RIP slot for consistent layout with make_fcontext */
    /* return address from call is at RSP+0x40, we save it at RSP+0x38 */
    movq  0x40(%rsp), %rcx
    movq  %rcx, 0x38(%rsp)

    /* save current stack pointer (our context) in rax for return */
    movq  %rsp, %rax

    /* switch to target stack */
    movq  %rdi, %rsp

    /* load return address from target context */
    movq  0x38(%rsp), %r8

    /* restore FPU state */
    ldmxcsr  (%rsp)
    fldcw    0x4(%rsp)

    /* restore callee-saved registers */
    movq  0x8(%rsp), %r12
    movq  0x10(%rsp), %r13
    movq  0x18(%rsp), %r14
    movq  0x20(%rsp), %r15
    movq  0x28(%rsp), %rbx
    movq  0x30(%rsp), %rbp

    /* deallocate context-data from target stack */
    leaq  0x48(%rsp), %rsp

    /* prepare return value: transfer_t { fctx = rax, data = rdx } */
    movq  %rsi, %rdx

    /* pass transfer_t as first argument to context function */
    /* RDI = fctx (source context), RSI = data */
    movq  %rax, %rdi

    /* jump to target */
    jmp  *%r8

.size sh_jump_fcontext,.-sh_jump_fcontext

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 *
 * Switches to another context and executes a function on top of it.
 * Arguments:
 *   rdi - target context to switch to
 *   rsi - user data to pass
 *   rdx - function to execute on target stack
 * Returns:
 *   transfer_t from the ontop function
 */
.globl sh_ontop_fcontext
.type sh_ontop_fcontext,@function
.align 16
sh_ontop_fcontext:
    /* save ontop-function in r8 */
    movq  %rdx, %r8

    /* reserve space for context-data on current stack */
    leaq  -0x40(%rsp), %rsp

    /* save FPU state */
    stmxcsr  (%rsp)
    fnstcw   0x4(%rsp)

    /* save callee-saved registers */
    movq  %r12, 0x8(%rsp)
    movq  %r13, 0x10(%rsp)
    movq  %r14, 0x18(%rsp)
    movq  %r15, 0x20(%rsp)
    movq  %rbx, 0x28(%rsp)
    movq  %rbp, 0x30(%rsp)

    /* save return address at RIP slot for consistent layout with make_fcontext */
    movq  0x40(%rsp), %rcx
    movq  %rcx, 0x38(%rsp)

    /* save current stack pointer (our context) in rax for return */
    movq  %rsp, %rax

    /* switch to target stack */
    movq  %rdi, %rsp

    /* restore FPU state */
    ldmxcsr  (%rsp)
    fldcw    0x4(%rsp)

    /* restore callee-saved registers */
    movq  0x8(%rsp), %r12
    movq  0x10(%rsp), %r13
    movq  0x18(%rsp), %r14
    movq  0x20(%rsp), %r15
    movq  0x28(%rsp), %rbx
    movq  0x30(%rsp), %rbp

    /* deallocate context-data from target stack */
    leaq  0x40(%rsp), %rsp

    /* prepare transfer_t argument: { fctx = rax, data = rsi } */
    movq  %rsi, %rdx
    movq  %rax, %rdi

    /* keep return-address on stack and jump to ontop-function */
    jmp  *%r8

.size sh_ontop_fcontext,.-sh_ontop_fcontext

/* Mark that we don't need executable stack */
.section .note.GNU-stack,"",%progbits

#endif

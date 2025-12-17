/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * x86 (32-bit) System V ABI context switching for ELF (Linux, BSD)
 *
 * Based on boost::context by Oliver Kowalke (Boost Software License)
 */

/****************************************************************************************
 *                                                                                      *
 *  Context data layout (32 bytes = 0x20):                                              *
 *  ----------------------------------------------------------------------------------  *
 *  |   0x0   |   0x4   |   0x8   |   0xc   |   0x10  |  0x14   |  0x18   |  0x1c   |  *
 *  ----------------------------------------------------------------------------------  *
 *  | fc_mxcsr|fc_x87_cw|   EDI   |   ESI   |   EBX   |   EBP   |   EIP   | hidden  |  *
 *  ----------------------------------------------------------------------------------  *
 *                                                                                      *
 *  The 'hidden' slot stores the address where transfer_t should be written            *
 *  (struct return ABI on i386)                                                         *
 *                                                                                      *
 ****************************************************************************************/

#ifndef __x86_64__

.file "fcontext_x86_sysv_elf.S"
.text

/*
 * fcontext_t sh_make_fcontext(void* sp, size_t size, void (*fn)(transfer_t))
 *
 * i386 calling convention: args on stack
 *   [esp+4]  - stack pointer (top of allocated stack)
 *   [esp+8]  - stack size (unused)
 *   [esp+12] - pointer to context function
 * Returns:
 *   eax - new context handle
 */
.globl sh_make_fcontext
.type sh_make_fcontext,@function
.align 16
sh_make_fcontext:
    /* first arg == top of context-stack */
    movl 0x4(%esp), %eax

    /* reserve space for first argument of context-function */
    leal -0x8(%eax), %eax

    /* align stack pointer to 16-byte boundary */
    andl $-16, %eax

    /* reserve space for context-data (0x24 bytes) + alignment */
    leal -0x28(%eax), %eax

    /* save address of context-function in EBX slot */
    movl 0xc(%esp), %ecx
    movl %ecx, 0x10(%eax)

    /* save MMX control and status word */
    stmxcsr (%eax)
    /* save x87 control word */
    fnstcw 0x4(%eax)

    /* store address for transfer_t return value */
    leal 0x8(%eax), %ecx
    movl %ecx, 0x1c(%eax)

    /* compute address of trampoline */
    call 1f
1:  popl %ecx
    addl $.L_sh_trampoline-1b, %ecx
    /* save trampoline as return address */
    movl %ecx, 0x18(%eax)

    /* compute address of finish */
    call 2f
2:  popl %ecx
    addl $.L_sh_finish-2b, %ecx
    /* save finish as EBP (will be pushed by trampoline) */
    movl %ecx, 0x14(%eax)

    /* return pointer to context-data */
    ret

.L_sh_trampoline:
    /* transfer_t is on stack at esp, esp+4 */
    /* EDI = fctx, ESI = data from jump_fcontext */
    movl %edi, (%esp)
    movl %esi, 0x4(%esp)
    /* push finish address as return address */
    pushl %ebp
    /* jump to context-function (address in EBX) */
    jmp *%ebx

.L_sh_finish:
    /* compute GOT address for PIC */
    call 3f
3:  popl %ebx
    addl $_GLOBAL_OFFSET_TABLE_+[.-3b], %ebx
    /* exit with code 0 */
    xorl %eax, %eax
    movl %eax, (%esp)
    call _exit@PLT
    hlt

.size sh_make_fcontext,.-sh_make_fcontext

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * i386 calling convention: args on stack
 *   [esp+4] - target context
 *   [esp+8] - user data
 * Returns:
 *   transfer_t { eax = fctx, edx = data } via hidden pointer
 */
.globl sh_jump_fcontext
.type sh_jump_fcontext,@function
.align 16
sh_jump_fcontext:
    /* reserve space for context-data */
    leal -0x1c(%esp), %esp

    /* save FPU state */
    stmxcsr (%esp)
    fnstcw 0x4(%esp)

    /* save callee-saved registers */
    movl %edi, 0x8(%esp)
    movl %esi, 0xc(%esp)
    movl %ebx, 0x10(%esp)
    movl %ebp, 0x14(%esp)

    /* store current ESP in ECX (our context) */
    movl %esp, %ecx

    /* load target context (arg 1) */
    movl 0x24(%esp), %eax
    /* load user data (arg 2) */
    movl 0x28(%esp), %edx

    /* switch to target stack */
    movl %eax, %esp

    /* get hidden pointer for struct return */
    movl 0x1c(%esp), %eax
    /* write transfer_t: fctx = ecx, data = edx */
    movl %ecx, (%eax)
    movl %edx, 0x4(%eax)

    /* load return address */
    movl 0x18(%esp), %ecx

    /* restore FPU state */
    ldmxcsr (%esp)
    fldcw 0x4(%esp)

    /* restore callee-saved registers */
    movl 0x8(%esp), %edi
    movl 0xc(%esp), %esi
    movl 0x10(%esp), %ebx
    movl 0x14(%esp), %ebp

    /* deallocate context-data */
    leal 0x20(%esp), %esp

    /* jump to target */
    jmp *%ecx

.size sh_jump_fcontext,.-sh_jump_fcontext

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 */
.globl sh_ontop_fcontext
.type sh_ontop_fcontext,@function
.align 16
sh_ontop_fcontext:
    /* load ontop function pointer */
    movl 0xc(%esp), %eax
    movl %eax, -0x4(%esp)

    /* reserve space for context-data */
    leal -0x1c(%esp), %esp

    /* save FPU state */
    stmxcsr (%esp)
    fnstcw 0x4(%esp)

    /* save callee-saved registers */
    movl %edi, 0x8(%esp)
    movl %esi, 0xc(%esp)
    movl %ebx, 0x10(%esp)
    movl %ebp, 0x14(%esp)

    /* store current ESP in ECX */
    movl %esp, %ecx

    /* load target context */
    movl 0x24(%esp), %eax
    /* load user data */
    movl 0x28(%esp), %edx

    /* switch to target stack */
    movl %eax, %esp

    /* restore FPU state */
    ldmxcsr (%esp)
    fldcw 0x4(%esp)

    /* restore callee-saved registers */
    movl 0x8(%esp), %edi
    movl 0xc(%esp), %esi
    movl 0x10(%esp), %ebx
    movl 0x14(%esp), %ebp

    /* deallocate context-data */
    leal 0x1c(%esp), %esp

    /* get hidden pointer */
    movl (%esp), %eax
    /* write transfer_t */
    movl %ecx, (%eax)
    movl %edx, 0x4(%eax)

    /* load ontop function */
    movl 0x4(%esp), %ecx

    /* jump to ontop function */
    jmp *%ecx

.size sh_ontop_fcontext,.-sh_ontop_fcontext

/* Mark that we don't need executable stack */
.section .note.GNU-stack,"",%progbits

#endif

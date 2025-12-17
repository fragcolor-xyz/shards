/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * x86_64 Windows ABI context switching (PE/COFF)
 *
 * Based on boost::context by Oliver Kowalke and Thomas Sailer (Boost Software License)
 * Includes TEB (Thread Environment Block) management for proper Windows fiber support
 */

/*************************************************************************************
 *                                                                                   *
 *  Context data layout on stack (336 bytes = 0x150):                                *
 *                                                                                   *
 *  0x00 - 0x9f:  XMM6-XMM15 (160 bytes) - callee-saved on Windows                   *
 *  0xa0:         fc_mxcsr, fc_x87_cw                                                *
 *  0xa8:         alignment                                                          *
 *  0xb0:         fiber local storage                                                *
 *  0xb8:         deallocation stack (from TEB)                                      *
 *  0xc0:         stack limit (from TEB)                                             *
 *  0xc8:         stack base (from TEB)                                              *
 *  0xd0 - 0xff:  R12, R13, R14, R15, RDI, RSI, RBX, RBP                              *
 *  0x100:        RBX                                                                *
 *  0x108:        RBP                                                                *
 *  0x110:        hidden (transport_t address)                                       *
 *  0x118:        RIP (return address)                                               *
 *  0x120 - 0x13f: parameter area (32-byte shadow space)                             *
 *  0x140:        FCTX, DATA (transport_t)                                           *
 *                                                                                   *
 *************************************************************************************/

.file "fcontext_x86_64_ms_pe.S"
.text

/*
 * fcontext_t sh_make_fcontext(void* sp, size_t size, void (*fn)(transfer_t))
 *
 * Windows x64 calling convention:
 *   RCX - stack pointer (top of allocated stack)
 *   RDX - stack size
 *   R8  - pointer to context function
 * Returns:
 *   RAX - new context handle
 */
.p2align 4,,15
.globl sh_make_fcontext
.def sh_make_fcontext; .scl 2; .type 32; .endef
.seh_proc sh_make_fcontext
sh_make_fcontext:
.seh_endprologue

    /* first arg == top of context-stack */
    movq  %rcx, %rax

    /* align stack pointer to 16-byte boundary */
    andq  $-16, %rax

    /* reserve space for context-data on stack */
    /* on context-function entry: (RSP - 0x8) % 16 == 0 */
    leaq  -0x150(%rax), %rax

    /* save context-function address in RBX slot (will be jumped to via trampoline) */
    movq  %r8, 0x100(%rax)

    /* save top address of context stack as 'base' for TEB */
    movq  %rcx, 0xc8(%rax)

    /* compute bottom address of context stack (limit) */
    negq  %rdx
    leaq  (%rcx,%rdx), %rcx

    /* save bottom address as 'limit' for TEB */
    movq  %rcx, 0xc0(%rax)

    /* save as 'deallocation stack' for TEB */
    movq  %rcx, 0xb8(%rax)

    /* set fiber local storage to zero */
    xorq  %rcx, %rcx
    movq  %rcx, 0xb0(%rax)

    /* save MMX control and status word */
    stmxcsr 0xa0(%rax)
    /* save x87 control word */
    fnstcw  0xa4(%rax)

    /* compute address of transport_t and store in hidden field */
    leaq  0x140(%rax), %rcx
    movq  %rcx, 0x110(%rax)

    /* compute address of trampoline */
    leaq  .L_sh_trampoline(%rip), %rcx
    /* save as return address (entered after first jump_fcontext) */
    movq  %rcx, 0x118(%rax)

    /* compute address of finish */
    leaq  .L_sh_finish(%rip), %rcx
    /* save as RBP (pushed by trampoline for return after context-function) */
    movq  %rcx, 0x108(%rax)

    /* return pointer to context-data */
    ret

.L_sh_trampoline:
    /* fix stack alignment */
    pushq %rbp
    /* jump to context-function (address in RBX) */
    jmp  *%rbx

.L_sh_finish:
    /* align stack for _exit call (32-byte shadow space already reserved) */
    andq  $-32, %rsp
    /* exit code is zero */
    xorq  %rcx, %rcx
    /* exit application */
    call  _exit
    hlt

.seh_endproc

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * Windows x64 calling convention:
 *   RCX - hidden pointer for struct return (implicit)
 *   RDX - target context to switch to
 *   R8  - user data to pass
 * Returns:
 *   transfer_t via hidden pointer in RCX
 */
.p2align 4,,15
.globl sh_jump_fcontext
.def sh_jump_fcontext; .scl 2; .type 32; .endef
.seh_proc sh_jump_fcontext
sh_jump_fcontext:
.seh_endprologue

    /* reserve space for context-data */
    leaq  -0x118(%rsp), %rsp

    /* save XMM registers (callee-saved on Windows) */
    movaps  %xmm6,  0x00(%rsp)
    movaps  %xmm7,  0x10(%rsp)
    movaps  %xmm8,  0x20(%rsp)
    movaps  %xmm9,  0x30(%rsp)
    movaps  %xmm10, 0x40(%rsp)
    movaps  %xmm11, 0x50(%rsp)
    movaps  %xmm12, 0x60(%rsp)
    movaps  %xmm13, 0x70(%rsp)
    movaps  %xmm14, 0x80(%rsp)
    movaps  %xmm15, 0x90(%rsp)

    /* save MMX control and status word */
    stmxcsr 0xa0(%rsp)
    /* save x87 control word */
    fnstcw  0xa4(%rsp)

    /* load NT_TIB from TEB */
    movq  %gs:(0x30), %r10

    /* save fiber local storage */
    movq  0x20(%r10), %rax
    movq  %rax, 0xb0(%rsp)
    /* save current deallocation stack */
    movq  0x1478(%r10), %rax
    movq  %rax, 0xb8(%rsp)
    /* save current stack limit */
    movq  0x10(%r10), %rax
    movq  %rax, 0xc0(%rsp)
    /* save current stack base */
    movq  0x08(%r10), %rax
    movq  %rax, 0xc8(%rsp)

    /* save callee-saved GP registers */
    movq  %r12, 0xd0(%rsp)
    movq  %r13, 0xd8(%rsp)
    movq  %r14, 0xe0(%rsp)
    movq  %r15, 0xe8(%rsp)
    movq  %rdi, 0xf0(%rsp)
    movq  %rsi, 0xf8(%rsp)
    movq  %rbx, 0x100(%rsp)
    movq  %rbp, 0x108(%rsp)

    /* save hidden address of transport_t */
    movq  %rcx, 0x110(%rsp)

    /* preserve current RSP in R9 (will be returned as fctx) */
    movq  %rsp, %r9

    /* switch to target stack (RDX = target context) */
    movq  %rdx, %rsp

    /* restore XMM registers */
    movaps  0x00(%rsp), %xmm6
    movaps  0x10(%rsp), %xmm7
    movaps  0x20(%rsp), %xmm8
    movaps  0x30(%rsp), %xmm9
    movaps  0x40(%rsp), %xmm10
    movaps  0x50(%rsp), %xmm11
    movaps  0x60(%rsp), %xmm12
    movaps  0x70(%rsp), %xmm13
    movaps  0x80(%rsp), %xmm14
    movaps  0x90(%rsp), %xmm15

    /* restore MMX control and status word */
    ldmxcsr 0xa0(%rsp)
    /* restore x87 control word */
    fldcw   0xa4(%rsp)

    /* load NT_TIB from TEB */
    movq  %gs:(0x30), %r10

    /* restore fiber local storage */
    movq  0xb0(%rsp), %rax
    movq  %rax, 0x20(%r10)
    /* restore deallocation stack */
    movq  0xb8(%rsp), %rax
    movq  %rax, 0x1478(%r10)
    /* restore stack limit */
    movq  0xc0(%rsp), %rax
    movq  %rax, 0x10(%r10)
    /* restore stack base */
    movq  0xc8(%rsp), %rax
    movq  %rax, 0x08(%r10)

    /* restore callee-saved GP registers */
    movq  0xd0(%rsp), %r12
    movq  0xd8(%rsp), %r13
    movq  0xe0(%rsp), %r14
    movq  0xe8(%rsp), %r15
    movq  0xf0(%rsp), %rdi
    movq  0xf8(%rsp), %rsi
    movq  0x100(%rsp), %rbx
    movq  0x108(%rsp), %rbp

    /* restore hidden address of transport_t */
    movq  0x110(%rsp), %rax

    /* deallocate context-data */
    leaq  0x118(%rsp), %rsp

    /* pop return address */
    popq  %r10

    /* write transport_t to hidden pointer location */
    /* return source context (R9) */
    movq  %r9, 0x0(%rax)
    /* return data (R8) */
    movq  %r8, 0x8(%rax)

    /* pass transport_t address as first arg to context-function */
    movq  %rax, %rcx

    /* jump to target */
    jmp  *%r10

.seh_endproc

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 *
 * Windows x64 calling convention:
 *   RCX - hidden pointer for struct return
 *   RDX - target context
 *   R8  - user data
 *   R9  - ontop function
 */
.p2align 4,,15
.globl sh_ontop_fcontext
.def sh_ontop_fcontext; .scl 2; .type 32; .endef
.seh_proc sh_ontop_fcontext
sh_ontop_fcontext:
.seh_endprologue

    /* reserve space for context-data */
    leaq  -0x118(%rsp), %rsp

    /* save XMM registers */
    movaps  %xmm6,  0x00(%rsp)
    movaps  %xmm7,  0x10(%rsp)
    movaps  %xmm8,  0x20(%rsp)
    movaps  %xmm9,  0x30(%rsp)
    movaps  %xmm10, 0x40(%rsp)
    movaps  %xmm11, 0x50(%rsp)
    movaps  %xmm12, 0x60(%rsp)
    movaps  %xmm13, 0x70(%rsp)
    movaps  %xmm14, 0x80(%rsp)
    movaps  %xmm15, 0x90(%rsp)

    /* save MMX control and status word */
    stmxcsr 0xa0(%rsp)
    /* save x87 control word */
    fnstcw  0xa4(%rsp)

    /* load NT_TIB from TEB */
    movq  %gs:(0x30), %r10

    /* save fiber local storage */
    movq  0x20(%r10), %rax
    movq  %rax, 0xb0(%rsp)
    /* save current deallocation stack */
    movq  0x1478(%r10), %rax
    movq  %rax, 0xb8(%rsp)
    /* save current stack limit */
    movq  0x10(%r10), %rax
    movq  %rax, 0xc0(%rsp)
    /* save current stack base */
    movq  0x08(%r10), %rax
    movq  %rax, 0xc8(%rsp)

    /* save callee-saved GP registers */
    movq  %r12, 0xd0(%rsp)
    movq  %r13, 0xd8(%rsp)
    movq  %r14, 0xe0(%rsp)
    movq  %r15, 0xe8(%rsp)
    movq  %rdi, 0xf0(%rsp)
    movq  %rsi, 0xf8(%rsp)
    movq  %rbx, 0x100(%rsp)
    movq  %rbp, 0x108(%rsp)

    /* save hidden address of transport_t */
    movq  %rcx, 0x110(%rsp)

    /* preserve current RSP in RCX */
    movq  %rsp, %rcx

    /* switch to target stack */
    movq  %rdx, %rsp

    /* restore XMM registers */
    movaps  0x00(%rsp), %xmm6
    movaps  0x10(%rsp), %xmm7
    movaps  0x20(%rsp), %xmm8
    movaps  0x30(%rsp), %xmm9
    movaps  0x40(%rsp), %xmm10
    movaps  0x50(%rsp), %xmm11
    movaps  0x60(%rsp), %xmm12
    movaps  0x70(%rsp), %xmm13
    movaps  0x80(%rsp), %xmm14
    movaps  0x90(%rsp), %xmm15

    /* restore MMX control and status word */
    ldmxcsr 0xa0(%rsp)
    /* restore x87 control word */
    fldcw   0xa4(%rsp)

    /* load NT_TIB from TEB */
    movq  %gs:(0x30), %r10

    /* restore fiber local storage */
    movq  0xb0(%rsp), %rax
    movq  %rax, 0x20(%r10)
    /* restore deallocation stack */
    movq  0xb8(%rsp), %rax
    movq  %rax, 0x1478(%r10)
    /* restore stack limit */
    movq  0xc0(%rsp), %rax
    movq  %rax, 0x10(%r10)
    /* restore stack base */
    movq  0xc8(%rsp), %rax
    movq  %rax, 0x08(%r10)

    /* restore callee-saved GP registers */
    movq  0xd0(%rsp), %r12
    movq  0xd8(%rsp), %r13
    movq  0xe0(%rsp), %r14
    movq  0xe8(%rsp), %r15
    movq  0xf0(%rsp), %rdi
    movq  0xf8(%rsp), %rsi
    movq  0x100(%rsp), %rbx
    movq  0x108(%rsp), %rbp

    /* restore hidden address of transport_t */
    movq  0x110(%rsp), %rax

    /* deallocate context-data */
    leaq  0x118(%rsp), %rsp

    /* keep return-address on stack */

    /* write transport_t */
    movq  %rcx, 0x0(%rax)
    movq  %r8, 0x8(%rax)

    /* transport_t as first arg */
    movq  %rax, %rcx
    movq  %rax, %rdx

    /* jump to ontop function (R9) */
    jmp  *%r9

.seh_endproc

/* Export symbols for Windows linker */
.section .drectve
.ascii " -export:\"sh_make_fcontext\""
.ascii " -export:\"sh_jump_fcontext\""
.ascii " -export:\"sh_ontop_fcontext\""

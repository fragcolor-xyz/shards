/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2020 Fragcolor Pte. Ltd. */

/*
 * RISC-V context switching for ELF (Linux)
 * Unified implementation for both rv32 and rv64
 *
 * Inspired by corosensei (MIT License) and boost::context patterns
 */

/*******************************************************
 *                                                     *
 *  Context data layout:                               *
 *  RV64: 104 bytes (13 registers * 8 bytes)           *
 *  RV32: 52 bytes (13 registers * 4 bytes)            *
 *                                                     *
 *  Saved registers (callee-saved):                    *
 *  - s0-s11 (x8-x9, x18-x27): 12 GP registers         *
 *  - ra (x1): return address                          *
 *                                                     *
 *  Note: FP registers (fs0-fs11) not saved by default *
 *  to reduce context size. Enable SH_FCONTEXT_FP if   *
 *  needed for your application.                       *
 *                                                     *
 *******************************************************/

.file "fcontext_riscv_elf.S"

/* Macros for rv32/rv64 portability */
#if __riscv_xlen == 64
#  define SZREG  8
#  define REG_S  sd
#  define REG_L  ld
#  define CTX_SIZE  0x70   /* 14 registers * 8 = 112 bytes (aligned to 16) */
#elif __riscv_xlen == 32
#  define SZREG  4
#  define REG_S  sw
#  define REG_L  lw
#  define CTX_SIZE  0x40   /* 14 registers * 4 = 56 bytes (aligned to 16 = 64) */
#else
#  error "Unsupported RISC-V XLEN"
#endif

/* Register offsets in context */
#define CTX_RA   (0 * SZREG)
#define CTX_S0   (1 * SZREG)
#define CTX_S1   (2 * SZREG)
#define CTX_S2   (3 * SZREG)
#define CTX_S3   (4 * SZREG)
#define CTX_S4   (5 * SZREG)
#define CTX_S5   (6 * SZREG)
#define CTX_S6   (7 * SZREG)
#define CTX_S7   (8 * SZREG)
#define CTX_S8   (9 * SZREG)
#define CTX_S9   (10 * SZREG)
#define CTX_S10  (11 * SZREG)
#define CTX_S11  (12 * SZREG)
#define CTX_PC   (13 * SZREG)  /* Entry point / return address */

.text

/*
 * fcontext_t sh_make_fcontext(void* sp, size_t size, void (*fn)(transfer_t))
 *
 * RISC-V calling convention:
 *   a0 - stack pointer (top of allocated stack)
 *   a1 - stack size (unused)
 *   a2 - pointer to context function
 * Returns:
 *   a0 - new context handle
 */
.global sh_make_fcontext
.type sh_make_fcontext, @function
.align 2
sh_make_fcontext:
    /* Align stack pointer to 16-byte boundary */
    andi a0, a0, -16

    /* Reserve space for context data */
    addi a0, a0, -CTX_SIZE

    /* Store context-function address as PC (entry point) */
    REG_S a2, CTX_PC(a0)

    /* Compute address of finish routine and store as RA */
    /* When context-function returns, it will jump to finish */
    la t0, .L_sh_finish
    REG_S t0, CTX_RA(a0)

    /* Return pointer to context-data */
    ret

.L_sh_finish:
    /* Context-function returned - exit with code 0 */
    li a0, 0
    call _exit

.size sh_make_fcontext, .-sh_make_fcontext

/*
 * transfer_t sh_jump_fcontext(fcontext_t to, void* vp)
 *
 * RISC-V calling convention:
 *   a0 - target context to switch to
 *   a1 - user data to pass to target
 * Returns:
 *   transfer_t { a0 = fctx, a1 = data }
 */
.global sh_jump_fcontext
.type sh_jump_fcontext, @function
.align 2
sh_jump_fcontext:
    /* Reserve stack for context-data */
    addi sp, sp, -CTX_SIZE

    /* Save return address */
    REG_S ra, CTX_RA(sp)

    /* Save callee-saved GP registers s0-s11 */
    REG_S s0, CTX_S0(sp)
    REG_S s1, CTX_S1(sp)
    REG_S s2, CTX_S2(sp)
    REG_S s3, CTX_S3(sp)
    REG_S s4, CTX_S4(sp)
    REG_S s5, CTX_S5(sp)
    REG_S s6, CTX_S6(sp)
    REG_S s7, CTX_S7(sp)
    REG_S s8, CTX_S8(sp)
    REG_S s9, CTX_S9(sp)
    REG_S s10, CTX_S10(sp)
    REG_S s11, CTX_S11(sp)

    /* Save current SP as PC slot (return point for when we're resumed) */
    REG_S ra, CTX_PC(sp)

    /* Store current SP in t0 (will become return value a0) */
    mv t0, sp

    /* Switch to target stack */
    mv sp, a0

    /* Load PC (entry point / return address) */
    REG_L t1, CTX_PC(sp)

    /* Restore return address */
    REG_L ra, CTX_RA(sp)

    /* Restore callee-saved GP registers s0-s11 */
    REG_L s0, CTX_S0(sp)
    REG_L s1, CTX_S1(sp)
    REG_L s2, CTX_S2(sp)
    REG_L s3, CTX_S3(sp)
    REG_L s4, CTX_S4(sp)
    REG_L s5, CTX_S5(sp)
    REG_L s6, CTX_S6(sp)
    REG_L s7, CTX_S7(sp)
    REG_L s8, CTX_S8(sp)
    REG_L s9, CTX_S9(sp)
    REG_L s10, CTX_S10(sp)
    REG_L s11, CTX_S11(sp)

    /* Deallocate context-data from stack */
    addi sp, sp, CTX_SIZE

    /* Prepare return value: transfer_t { a0 = fctx, a1 = data } */
    /* a1 already contains data, move saved SP to a0 */
    mv a0, t0

    /* Jump to target (entry point or return address) */
    jr t1

.size sh_jump_fcontext, .-sh_jump_fcontext

/*
 * transfer_t sh_ontop_fcontext(fcontext_t to, void* vp, transfer_t (*fn)(transfer_t))
 *
 * RISC-V calling convention:
 *   a0 - target context
 *   a1 - user data
 *   a2 - ontop function
 * Returns:
 *   transfer_t from ontop function
 */
.global sh_ontop_fcontext
.type sh_ontop_fcontext, @function
.align 2
sh_ontop_fcontext:
    /* Save ontop function pointer in t2 */
    mv t2, a2

    /* Reserve stack for context-data */
    addi sp, sp, -CTX_SIZE

    /* Save return address */
    REG_S ra, CTX_RA(sp)

    /* Save callee-saved GP registers s0-s11 */
    REG_S s0, CTX_S0(sp)
    REG_S s1, CTX_S1(sp)
    REG_S s2, CTX_S2(sp)
    REG_S s3, CTX_S3(sp)
    REG_S s4, CTX_S4(sp)
    REG_S s5, CTX_S5(sp)
    REG_S s6, CTX_S6(sp)
    REG_S s7, CTX_S7(sp)
    REG_S s8, CTX_S8(sp)
    REG_S s9, CTX_S9(sp)
    REG_S s10, CTX_S10(sp)
    REG_S s11, CTX_S11(sp)

    /* Save current SP as PC slot */
    REG_S ra, CTX_PC(sp)

    /* Store current SP in t0 */
    mv t0, sp

    /* Switch to target stack */
    mv sp, a0

    /* Restore return address */
    REG_L ra, CTX_RA(sp)

    /* Restore callee-saved GP registers s0-s11 */
    REG_L s0, CTX_S0(sp)
    REG_L s1, CTX_S1(sp)
    REG_L s2, CTX_S2(sp)
    REG_L s3, CTX_S3(sp)
    REG_L s4, CTX_S4(sp)
    REG_L s5, CTX_S5(sp)
    REG_L s6, CTX_S6(sp)
    REG_L s7, CTX_S7(sp)
    REG_L s8, CTX_S8(sp)
    REG_L s9, CTX_S9(sp)
    REG_L s10, CTX_S10(sp)
    REG_L s11, CTX_S11(sp)

    /* Deallocate context-data */
    addi sp, sp, CTX_SIZE

    /* Prepare transfer_t argument: { a0 = fctx, a1 = data } */
    mv a0, t0

    /* Jump to ontop function */
    jr t2

.size sh_ontop_fcontext, .-sh_ontop_fcontext

/* Mark that we don't need executable stack */
.section .note.GNU-stack,"",@progbits

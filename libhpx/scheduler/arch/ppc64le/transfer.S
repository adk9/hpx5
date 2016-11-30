// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include "../common/asm_macros.h"
#include "ppc-asm.h"

        // @param r3 - a pointer to the stack pointer to transfer to
        // @param r4 - the continuation to call after the sp exchange
        // @param r5 - the worker
        .file "libhpx/scheduler/arch/ppc64le/transfer.S"
        .machine power8
        .abiversion 2
        .section        ".toc","aw"
        .section        .text
        .type thread_transfer, STT_FUNC
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        STARTPROC
        # PROLOGUE
        # 1) Reserve stack space and save the old stack pointer
        # This will allocate stack memory atomically.
        stdu 1,-264(1)
        # Save LR
        mflr 0
        std  0, 0*8(1)
        # store r2 (TOC)
        std  2, 1*8(1)

        # Save all non-volatile registers that will be used
        # during this function
        std  14, 3*8(1)
        std  15, 4*8(1)
        std  16, 5*8(1)
        std  17, 6*8(1)
        std  18, 7*8(1)
        std  19, 8*8(1)
        std  20, 9*8(1)
        std  21, 10*8(1)
        std  22, 11*8(1)
        std  23, 12*8(1)
        std  24, 13*8(1)
        std  25, 14*8(1)
        std  26, 15*8(1)
        std  27, 16*8(1)
        std  28, 17*8(1)
        std  29, 18*8(1)
        std  30, 19*8(1)
        std  31, 20*8(1)

        // r0 commonly used to hold the old link register when building the stack frame
        // r1 is the stack pointer 
        // r2 dedicated table of contents pointer
        // r3 Volatile parameter and return value register
        // r4-r10 Volatile registers used for function parameters
        // We want to call cont(p, f, w, sp).
        // p is there in r3, f in r4 and this is in r5
        // but we need to move the new sp to x6.
        mr 6, 1     // Checkpoint sp for call
        ld 7, 0(3)  // p->ustack r7 = (r3)
        ld 7, 0(7)  // p->ustack->sp r7 = (r7)
        mr 1, 7     // Replace r1 (sp) for the context switch

        // Make enough space for the worker_checkpoint to store sp to the frame
        stdu 1, -8(1)
        bl worker_checkpoint
        addi 1, 1, 8
        # move old return address into the link register
        ld   0, 0*8(1)
        mtlr 0
        # restore r2 (TOC)
        ld   2, 1*8(1)
        ld   3, 2*8(1)
        # The function's epilogue follows the reverse sequence, restoring what had been saved
        ld  14, 3*8(1)
        ld  15, 4*8(1)
        ld  16, 5*8(1)
        ld  17, 6*8(1)
        ld  18, 7*8(1)
        ld  19, 8*8(1)
        ld  20, 9*8(1)
        ld  21, 10*8(1)
        ld  22, 11*8(1)
        ld  23, 12*8(1)
        ld  24, 13*8(1)
        ld  25, 14*8(1)
        ld  26, 15*8(1)
        ld  27, 16*8(1)
        ld  28, 17*8(1)
        ld  29, 18*8(1)
        ld  30, 19*8(1)
        ld  31, 20*8(1)

        # Restore stack frame
        addi 1, 1, 264

        # Return
        blr
        ENDPROC
        SIZE(thread_transfer)

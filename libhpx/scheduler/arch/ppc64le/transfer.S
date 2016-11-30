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
        stdu r1, -264(r1)
        # Save LR
        mflr r0
        std  r0, 0*8(r1)
        # store r2 (TOC)
        std   r2,1*8(r1)

        # Save all non-volatile registers that will be used
        # during this function
        std  r14,3*8(r1)
        std  r15,4*8(r1)
        std  r16,5*8(r1)
        std  r17,6*8(r1)
        std  r18,7*8(r1)
        std  r19,8*8(r1)
        std  r20,9*8(r1)
        std  r21,10*8(r1)
        std  r22,11*8(r1)
        std  r23,12*8(r1)
        std  r24,13*8(r1)
        std  r25,14*8(r1)
        std  r26,15*8(r1)
        std  r27,16*8(r1)
        std  r28,17*8(r1)
        std  r29,18*8(r1)
        std  r30,19*8(r1)
        std  r31,20*8(r1)

        // r0 commonly used to hold the old link register when building the stack frame
        // r1 is the stack pointer 
        // r2 dedicated table of contents pointer
        // r3 Volatile parameter and return value register
        // r4-r10 Volatile registers used for function parameters
        // We want to call cont(p, f, w, sp).
        // p is there in r3, f in r4 and this is in r5
        // but we need to move the new sp to x6.
        mr r6, r1     // Checkpoint sp for call
        ld r7, 0(r3)  // p->ustack r7 = (r3)
        ld r7, 0(r7)  // p->ustack->sp r7 = (r7)
        mr r1, r7     // Replace r1 (sp) for the context switch

	// Make enough space for the worker_checkpoint to store sp to the frame.
	stdu r1, -8(r1)
        bl worker_checkpoint
        addi r1, r1, 8
        # move old return address into the link register
        ld   r0,0*8(r1)
        mtlr r0
        # restore r1-r2 (SP and TOC)
        ld  r2, 1*8(r1)
        ld  r3, 2*8(r1)
        # The function's epilogue follows the reverse sequence, restoring what had been saved
        ld  r14,3*8(r1)
        ld  r15,4*8(r1)
        ld  r16,5*8(r1)
        ld  r17,6*8(r1)
        ld  r18,7*8(r1)
        ld  r19,8*8(r1)
        ld  r20,9*8(r1)
        ld  r21,10*8(r1)
        ld  r22,11*8(r1)
        ld  r23,12*8(r1)
        ld  r24,13*8(r1)
        ld  r25,14*8(r1)
        ld  r26,15*8(r1)
        ld  r27,16*8(r1)
        ld  r28,17*8(r1)
        ld  r29,18*8(r1)
        ld  r30,19*8(r1)
        ld  r31,20*8(r1)

        # Restore stack frame
        addi   r1, r1, 264

        # Return
        blr
        ENDPROC
        SIZE(thread_transfer)

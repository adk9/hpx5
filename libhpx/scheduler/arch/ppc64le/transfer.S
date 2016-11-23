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
        .text
        .type thread_transfer, STT_FUNC
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        STARTPROC

        std   r1,-264(r1)
        std   r2,-256(r1)
        std  r14,-248(r1)
        std  r15,-240(r1)
        std  r16,-232(r1)
        std  r17,-224(r1)
        std  r18,-216(r1)
        std  r19,-208(r1)
        std  r20,-200(r1)
        std  r21,-192(r1)
        std  r22,-184(r1)
        std  r23,-176(r1)
        std  r24,-168(r1)
        std  r25,-160(r1)
        std  r26,-152(r1)
        std  r27,-144(r1)
        std  r28,-136(r1)
        std  r29,-128(r1)
        std  r30,-120(r1)
        std  r31,-112(r1)
        std   r0,-104(r1)
        mflr  r0

        // r0 commonly used to hold the old link register when building the stack frame
        // r1 is the stack pointer 
        // r2 dedicated table of contents pointer
        // r3 Volatile parameter and return value register
        // r4-r10 Volatile registers used for function parameters
        // We want to call cont(p, f, w, sp).
        // p is there in r3, f in r4 and this is in r5
        // but we need to move the new sp to x6.
        mr r6, r1     // Checkpoint sp for call
        la r7, 0(r3)  // p->ustack r7 = (r3)
        la r7, 0(r7)  // p->ustack->sp r7 = (r7)
        mr r1, r7     // Replace r1 (sp) for the context switch
        bl worker_checkpoint

        ld   r1,-264(r1)
        ld   r2,-256(r1)
        ld  r14,-248(r1)
        ld  r15,-240(r1)
        ld  r16,-232(r1)
        ld  r17,-224(r1)
        ld  r18,-216(r1)
        ld  r19,-208(r1)
        ld  r20,-200(r1)
        ld  r21,-192(r1)
        ld  r22,-184(r1)
        ld  r23,-176(r1)
        ld  r24,-168(r1)
        ld  r25,-160(r1)
        ld  r26,-152(r1)
        ld  r27,-144(r1)
        ld  r28,-136(r1)
        ld  r29,-128(r1)
        ld  r30,-120(r1)
        ld  r31,-112(r1)
        ld   r0,-104(r1)
  
        mtlr r14
        # Return
        blr
        ENDPROC
        SIZE(thread_transfer)

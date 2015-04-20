// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#include "asm_macros.h"

        // @param r0 - a pointer to the stack pointer to transfer to
        // @param r1 - the continuation to call after the sp exchange
        // @param r2 - the environment for the continutation
        // @returns r0  -  from the continuation call
        .file "libhpx/scheduler/arch/arch/transfer.S"
        .text
        .syntax unified
        .code 32
        .type thread_transfer, STT_FUNC
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
        LABEL(thread_transfer)
        push {lr}
	push {r4}
	push {r5}
        stmdb sp!, {r6-r11} // may not need r9?
        sub sp, sp, #4 // keep stack alignment

#if __VFP_FP__
	// if VFP and/or Advanced SIMD (i.e. NEON) is enabled, we need
	// to back up the floating point registers and the control
	// register.
	// We can check for NEON support directly with __ARM_NEON__ but
	// I believe __VFP_FP__ is set whenever __ARM_NEON__ is set. The
	// registers are the same.
	vstmdb sp!, {s16-s31}
        sub sp, sp, #8      // could be 4 but we want to keep stack 8 byte aligned
        vmrs r5, fpscr
        str r5, [sp, #8]
#endif
        mov r4, r1                                  // cont          r4 = r1
        ldr r1, [r0]                                // p->ustack     r1 = (r0)
        ldr r1, [r1]                                // p->ustack->sp  r1 = (r1)
        mov r5, sp                                  // swap sp and r1
        mov sp, r1                                  // ... cont.
        mov r1, r5                                  // ... cont.

        blx r4                                      // cont(p, sp, env)
                                                    // (p is still in r0 and env is still in r2)
#ifdef __VFP_FP__       
        ldr r5, [sp, #8]
        add sp, sp, #8
        vldmia sp!, {s16-s31}
#endif
        add sp, sp, #4
        ldmia sp!, {r6-r11}
	pop {r5}
	pop {r4}
        pop {lr}
        
        bx lr  
        SIZE(thread_transfer)

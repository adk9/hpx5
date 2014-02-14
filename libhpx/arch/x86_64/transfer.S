// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#include "asm_macros.h"
        
        .file "transfer.S"
/// ----------------------------------------------------------------------------
/// There is an asymmetry in this checkpoint code. On the way in, we do not
/// checkpoint %rdi (it's caller-saves), but pop it on the way out. We make sure
/// the stack spack is symmetric by subtracting 16 bytes on the way in, but only
/// 8 bytes on the way out (plus the 8 byte pop).
///
/// The reason that we do this has to do with how threads start execution. The
/// thread-entry function expects to be passed an hpx_parcel_t * as its first
/// parameter (in %rdi). As an efficiency mechanism, we initialize the first
/// frame for a stack with the correct hpx_parcel_t * as the "checkpointed"
/// %rdi. The "pop %rdi" on the way out of the original transfer moves that into
/// the right register, so that the entry function works.
///
/// There are two obvious alternatives to this.
/// 
/// 1) We could change the entry function type to void (*)(void), and force it
///    to reload the parcel from the current stack. This is extra work on each
///    new thread, which we can easily avoid.
///
/// 2) We could use two of the callee-saves registers that we're already saving,
///    and use a trampoline to actually start each thread. For instance, if we
///    used %rbx as the hpx_parcel_t * and %r12 as the "actual" entry function,
///    then we would have a trampoline like:
///
/// 	    INTERNAL(_ustack_entry_trampoline)
///         GLOBAL(_ustack_entry_trampoline)
///     LABEL(_ustack_entry_trampoline)
///         mov %rbx, %rdi
///         jmp %r12
///         SIZE(_ustack_entry_trampoline)
///
/// The entry trampoline would work fine, but would again add overhead to thread
/// startup, in this case an indirect branch. The selected mechanism adds one
/// "pop %rdi" to each context switch, and means that context switches require
/// one more word of stack space, but optimizes thread startup. Startup times
/// are our fastpath, so that's where we optimize. The trampoline can be
/// evaluated later if we need to slightly diminish context switch time or
/// checkpoint space.
/// ----------------------------------------------------------------------------
	    .text
	    INTERNAL(ustack_transfer)
        GLOBAL(ustack_transfer)
LABEL(ustack_transfer)
	    push %rbp
	    push %rbx
	    push %r12
	    push %r13
	    push %r14
	    push %r15
        ;; push %rdi
	    sub $16, %rsp
	    fnstcw 4(%rsp)
	    stmxcsr	(%rsp)
	    xchg %rsp, %rdi
	    call *%rsi
	    ldmxcsr	(%rsp)
	    fldcw 4(%rsp)
	    add $8, %rsp
        pop %rdi
	    pop	%r15
        pop	%r14
	    pop	%r13
	    pop	%r12
	    pop	%rbx
	    pop	%rbp
        ret
        SIZE(ustack_transfer)

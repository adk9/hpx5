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
        
        /// --------------------------------------------------------------------
        /// The x86_64 transfer implementation.
        ///
        /// Checkpoints the current callee-saves state on the current thread's
        /// stack, then exchanges stack pointers to start (or resume) execution
        /// on the target stack, then calls the continuation with the old stack
        /// pointer, and restores the checkpoint from the target thread's
        /// stack.
        ///
        /// @todo: This has to 100% match with the _frame_t in thread.c. We
        ///        should extract that type and its functionality into a
        ///        platform-specific object file that lives here.
        ///
        /// There is an asymmetry in this checkpoint code. On the way in, we do
        /// not checkpoint %rdi (it's caller-saves), but pop it on the way
        /// out. We make sure the stack space is symmetric by subtracting 16
        /// bytes on the way in, but only 8 bytes on the way out (plus the 8
        /// extra byte pop of %rdi).
        ///
        /// The reason that we do this has to do with how threads start
        /// execution. The thread-entry function expects to be passed an
        /// hpx_parcel_t * as its first parameter (in %rdi). As an efficiency
        /// mechanism, we initialize the first frame for a stack with the
        /// correct hpx_parcel_t * as the "checkpointed" %rdi. The "pop %rdi" on
        /// the way out of the original transfer moves that into the right
        /// register, so that the entry function works. 
        ///
        /// There are two obvious alternatives to this.
        /// 
        /// 1) We could change the entry function type to void (*)(void), and
        ///    force it to reload the parcel from the current stack. This is
        ///    extra work on each new thread, which we can easily avoid by using
        ///    the one extra stack slot and common-case pop.
        ///
        /// 2) We could use two of the callee-saves registers that we're already
        ///    saving, and use a trampoline to actually start each thread. For
        ///    instance, if we used %rbx as the hpx_parcel_t * and %r12 as the
        ///    "actual" entry function, then we would have a trampoline like:
        ///
        ///     INTERNAL(_thread_entry_trampoline)
        ///     GLOBAL(_thread_entry_trampoline)
        ///     LABEL(_thread_entry_trampoline)
        ///       mov %rbx, %rdi
        ///       jmp %r12
        ///     SIZE(_thread_entry_trampoline)
        ///
        /// The entry trampoline would work fine, but would again add overhead
        /// to thread startup, in this case an indirect branch. The selected
        /// mechanism adds one "pop %rdi" to each context switch, and means that
        /// context switches require one more word of stack space, but optimizes
        /// thread startup. Startup times are our fastpath, so that's where we
        /// optimize. The trampoline can be evaluated later if we need to
        /// slightly diminish context switch time or checkpoint space.
        ///
        /// @param %rdi - a pointer to the stack pointer to transfer to
        /// @param %rsi - the continuation to call after the sp exchange
        /// @param %rdx - the environment for the continutation
        /// @returns    - %rax from the continuation call
        /// --------------------------------------------------------------------
        .file "transfer.S"
        .text
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        push %rbp                               // 1
        push %rbx                               // 2
        push %r12                               // 3
        push %r13                               // 4
        push %r14                               // 5
        push %r15                               // 6
        //push %rdi                             // 7
        sub $24, %rsp                           // 
        fnstcw 4(%rsp)                          // 8.5
        stmxcsr	(%rsp)                          // 8
        mov %rsi, %rbx                          // cont
        mov (%rdi), %rsi                        // p->stack
        and $0xfffffffffffffffc, %rsi           // p->stack & ~sizeof(void*)
        mov (%rsi), %rsi                        // p->stack->sp
        xchg %rsp, %rsi
        call *%rbx                              // cont(p, sp, env)
        ldmxcsr	(%rsp)                          // 8
        fldcw 4(%rsp)                           // 8.5
        add $16, %rsp                           // 
        pop %rdi                                // 7
        pop %r15                                // 6
        pop %r14                                // 5
        pop %r13                                // 4
        pop %r12                                // 3
        pop %rbx                                // 2
        pop %rbp                                // 1
        ret
        SIZE(thread_transfer)

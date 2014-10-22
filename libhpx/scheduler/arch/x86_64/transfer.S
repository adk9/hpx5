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
        
        /// The x86_64 transfer implementation.
        ///
        /// Checkpoints the current callee-saves state on the current thread's
        /// stack, then exchanges stack pointers to start (or resume) execution
        /// on the target stack, then calls the continuation with the old stack
        /// pointer, and restores the checkpoint from the target thread's
        /// stack.
        ///
        /// For newly allocated stacks, we have to "forge" a thread_transfer()
        /// frame that we can start up through a thread_transfer() to it. We
        /// know what the frame looks like, so we can manufacture one. The only
        /// sneakiness in doign this is that we need to make sure that the stack
        /// pointer is aligned correctly, which is what the
        /// _stack_align_trampoline is for.
        ///
        /// @todo: This has to 100% match with the _frame_t in thread.c. We
        ///        should extract that type and its functionality into a
        ///        platform-specific object file that lives here.
        ///
        /// @param %rdi A pointer to the stack pointer to transfer to.
        /// @param %rsi The continuation to call after the sp exchange.
        /// @param %rdx The environment for the continutation.
        ///
        /// @returns %rax from the continuation call

        .file "libhpx/scheduler/arch/x86_64/transfer.S"
        .text
        INTERNAL(_stack_align_trampoline)
        GLOBAL(_stack_align_trampoline)
LABEL(_stack_align_trampoline)
        mov %r12, %rdi
        call *%rbx
        SIZE(_stack_align_trampoline)

        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        push %rbp                               // 1
        push %rbx                               // 2
        push %r12                               // 3
        push %r13                               // 4
        push %r14                               // 5
        push %r15                               // 6
        sub $8, %rsp                            // 7
        fnstcw 4(%rsp)                          // 7.5
        stmxcsr	(%rsp)                          // 7
        mov %rsi, %rbx                          // cont
        mov (%rdi), %rsi                        // p->stack
        and $0xfffffffffffffffc, %rsi           // p->stack & ~sizeof(void*)
        mov (%rsi), %rsi                        // p->stack->sp
        xchg %rsp, %rsi
        call *%rbx                              // cont(p, sp, env)
        ldmxcsr	(%rsp)                          // 7
        fldcw 4(%rsp)                           // 7.5
        add $8, %rsp                            // 
        pop %r15                                // 6
        pop %r14                                // 5
        pop %r13                                // 4
        pop %r12                                // 3
        pop %rbx                                // 2
        pop %rbp                                // 1
        ret
        SIZE(thread_transfer)


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
        /// align_rsp_trampoline is for. See frame.c for details.
        ///
        /// The stack pointer pointer may have some extra bits set in its least
        /// significant bits, so we mask them out before trying to load from it.
        ///
        /// @param %rdi A (mangled) pointer to the stack pointer to transfer to.
        /// @param %rsi The continuation to call after the sp exchange.
        /// @param %rdx The environment for the continutation.
        ///
        /// @returns %rax from the continuation call

        .file "transfer.S"
        .text
        .section .rodata
        TYPE(thread_transfer, @function)
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        STARTPROC
        push %rbp
        CFA_OFFSET(16)
        push %rbx
        CFA_OFFSET(24)
        push %r12
        CFA_OFFSET(32)
        push %r13
        CFA_OFFSET(40)
        push %r14
        CFA_OFFSET(48)
        push %r15
        CFA_OFFSET(56)
        sub $8, %rsp
        CFA_OFFSET(64)
        fnstcw 4(%rsp)
        stmxcsr	(%rsp)
        mov %rsi, %rbx                          // cont
        mov (%rdi), %rsi                        // p->stack
        and $0xfffffffffffffffc, %rsi           // p->stack & ~sizeof(void*)
        mov (%rsi), %rsi                        // p->stack->sp
        xchg %rsp, %rsi
        call *%rbx                              // cont(p, sp, env)
        ldmxcsr	(%rsp)
        fldcw 4(%rsp)
        add $8, %rsp
        CFA_OFFSET(56)
        pop %r15
        CFA_OFFSET(48)
        pop %r14
        CFA_OFFSET(40)
        pop %r13
        CFA_OFFSET(32)
        pop %r12
        CFA_OFFSET(24)
        pop %rbx
        CFA_OFFSET(16)
        pop %rbp
        CFA_OFFSET(8)
        ret
        ENDPROC
        SIZE(thread_transfer)



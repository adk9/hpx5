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
        
/// Implementation of some simple asm utilities.
///
/// These could be done simply as inline asm if we only wanted to support
/// gcc-compatible inline asm implementations, but we care about CrayCC and we
/// haven't seen such support there yet. This costs us a function call (and
/// associated caller-side register churn) for these operations.
        .file "libhpx/scheduler/arch/x86_64/asm.S"

	TYPE(get_mxcsr, @function)
	INTERNAL(get_mxcsr)
	GLOBAL(get_mxcsr)
LABEL(get_mxcsr)
        STARTPROC
        CFA_DEF_OFFSET(8)
	    stmxcsr (%rdi)
	    ret
        ENDPROC
	    SIZE(get_mxcsr)

	TYPE(get_fpucw, @function)
        INTERNAL(get_fpucw)
	    GLOBAL(get_fpucw)
LABEL(get_fpucw)
        STARTPROC
        CFA_DEF_OFFSET(8)
	    fnstcw (%rdi)
	    ret
        ENDPROC
        SIZE(get_fpucw)

        // This label is a complete hack. It's just here so that gdb outputs
        // some useful initial thread_transfer information on bt, without it,
        // thinks that the outer frame is get_fpucw. With it, it will report
        // this symbol name.
        //INTERNAL(_align_rsp_trampoline_new_thread_gdb_label_hack)
// LABEL(_align_rsp_trampoline_new_thread_gdb_label_hack)
//         nop
//         SIZE(_align_rsp_trampoline_new_thread_gdb_label_hack)

	TYPE(align_rsp_trampoline, @function)
        INTERNAL(align_rsp_trampoline)
        GLOBAL(align_rsp_trampoline)
LABEL(align_rsp_trampoline)
        STARTPROC
        mov %r12, %rdi
        call *%rbx
        ENDPROC
        SIZE(align_rsp_trampoline)


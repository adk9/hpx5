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

        /// Implementation of some simple asm utilities.
        ///
        /// These could be done simply as inline asm if we only wanted to
        /// support gcc-compatible inline asm implementations, but we care about
        /// CrayCC and we haven't seen such support there yet. This costs us a
        /// function call (and associated caller-side register churn) for these
        /// operations.
        .file "libhpx/scheduler/arch/ppc64le/asm.S"
        .machine power8
        .abiversion 2
        .section        ".toc","aw"
        .text
        .type align_stack_trampoline, STT_FUNC
        INTERNAL(align_stack_trampoline)
        GLOBAL(align_stack_trampoline)
        .type align_stack_trampoline, %function

LABEL(align_stack_trampoline)
        STARTPROC
        mr 3, 15
        mtlr 14
        blr
        ENDPROC
        SIZE(align_stack_trampoline)

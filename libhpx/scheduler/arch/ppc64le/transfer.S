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

        // @param r0 - a pointer to the stack pointer to transfer to
        // @param r1 - the continuation to call after the sp exchange
        // @param r2 - the worker
        .file "libhpx/scheduler/arch/ppc64le/transfer.S"
        .text
        .type thread_transfer, STT_FUNC
        INTERNAL(thread_transfer)
        GLOBAL(thread_transfer)
LABEL(thread_transfer)
        STARTPROC
        // Coming soon...
        ENDPROC
        SIZE(thread_transfer)

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
        
        .file "asm.S"
/// ----------------------------------------------------------------------------
/// Implementation of some simple asm utilities.
///
/// These could be done simply as inline asm if we only wanted to support
/// gcc-compatible inline asm implementations, but we care about CrayCC and we
/// haven't seen such support there yet. This costs us a function call (and
/// associated caller-side register churn) for these operations. 
/// ----------------------------------------------------------------------------
	    INTERNAL(get_mxcsr)
	    GLOBAL(get_mxcsr)
LABEL(get_mxcsr)
	    stmxcsr (%rdi)
	    ret
	    SIZE(get_mxcsr)

        INTERNAL(get_fpucw)
	    GLOBAL(get_fpucw)
LABEL(get_fpucw)
	    fnstcw (%rdi)
	    ret
        SIZE(get_fpucw)

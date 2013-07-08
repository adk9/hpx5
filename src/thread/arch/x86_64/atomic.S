# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  Atomic Operations
#  hpx_atomic.S
# 
#  Copyright (c) 2013, Trustees of Indiana University
#  All rights reserved.
# 
#  This software may be modified and distributed under the terms of
#  the BSD license.  See the COPYING file for details.
# 
#  This software was created at the Indiana University Center for
#  Research in Extreme Scale Technologies (CREST).
#
#  Authors:
#    Patrick K. Bohan <pbohan [at] indiana.edu>
# ====================================================================

#include "arch/x86_64/mconfig_defs.h"
	
.intel_syntax noprefix
	
	
.align 8
.globl HPX_CDECL(hpx_atomic_set8)

#ifdef __linux__
.type HPX_CDECL(hpx_atomic_set8), %function
#endif
	
.text
	
	
# --------------------------------------------------------------------
#  hpx_atomic_set8
#
#  Atomically stores a byte of data to the provided memory
#  location.
# --------------------------------------------------------------------

HPX_CDECL(hpx_atomic_set8):
	movq			rax,rsi
	xchg			[rdi],al
	ret


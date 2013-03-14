
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  Machine Configuration Routines
#  hpx_mctx.S
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

#include "arch/x86_64/hpx_mconfig_defs.h"
	
.intel_syntax

.globl _hpx_mconfig_get_flags

	
.text

# --------------------------------------------------------------------
#  hpx_mconfig_get_flags
#
#  Fetches CPU features that we're interested in.
# --------------------------------------------------------------------
	
_hpx_mconfig_get_flags:
	# we have to save RBX on Darwin (Mac OS X) because the
	# opcode generated for CPUID obliterates it.
#ifdef __APPLE__	
	movq	r10,rbx
#endif
	
	# assume we can execute CPUID since we are on an x86_64
	# architecture CPU.  this might be naive. 
	mov	eax,1
	cpuid

#ifdef __APPLE__	
	xchgq	rbx,r10
#endif

	xor	r9,r9
	
	# do we have an FPU?
	test	edx,0x00000001
	jz	no_fpu
	add	r9,_HPX_MCONFIG_HAS_FPU
	
	# do we have an FXSR?
	test	edx,0x01000000
	jz	no_fxsr
	add	r9,_HPX_MCONFIG_HAS_FXSR

	# do we have AVX instructions & registers?
no_fxsr:	
	test	ecx,0x10000000
	jz	no_avx
	add	r9,_HPX_MCONFIG_HAS_AVX

	# do we have a time-stamp counter?
no_fpu:	
	test	edx,0x00000010
	jz	no_tsc
	add	r9,_HPX_MCONFIG_HAS_TSC

no_tsc:
	movq	rax,r9
	ret

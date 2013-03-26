
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  Unit Test Helpers
#  99_hpxtest.S
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

.align 8
.globl __fpu_crusher, __sse_crusher

.text
	
	
# --------------------------------------------------------------------
#  __fpu_crusher
#
#  Put some stuff to test in the FPU (if we have one).
# --------------------------------------------------------------------

__fpu_crusher:
	push		rbp
	movq		rbp,rsp

	# see if we have an FPU
	test		rdi,_HPX_MCONFIG_HAS_FPU
	jz		t_no_fpu

	# load up some registers and push them on the FPU stack
	finit
	fldpi
	fld1
	fldl2e
	fldz
	fld1

	# do a trivial calculation
	f2xm1
	
t_no_fpu:
	movq		rsp,rbp
	popq		rbp
	ret

	
# --------------------------------------------------------------------
#  __sse_crusher
#
#  Put some stuff to test in the SSE registers (if we have them).
# --------------------------------------------------------------------

__sse_crusher:
	push		rbp
	movq		rbp,rsp

	# see if we have SSE
	test		rdi,_HPX_MCONFIG_HAS_SSE
	jz		t_no_sse
	
	# load our arguments into XMM registers 0-7
	leaq		rax,[rdx]
	movaps		xmm0,[rax]

	movlps		xmm1,[rax+8]
	movhps		xmm1,[rax]

	movaps		xmm2,xmm1
	maxps		xmm2,xmm0

	movaps		xmm3,xmm2
	subps		xmm3,xmm0

	movaps		xmm4,xmm0
	minps		xmm4,xmm1

	pxor		xmm5,xmm5
	addps		xmm5,xmm4
	
t_no_sse:
	movq		rsp,rbp
	popq		rbp
	ret

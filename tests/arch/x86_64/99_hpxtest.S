
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

	# put the best number in R11
	movq		r11,73
	
	# load our arguments into the XMM registers
	pxor		xmm0,xmm0
	pinsrq		xmm0,r11,0
	pinsrq		xmm0,rdx,1
	
	pxor		xmm1,xmm1
	pinsrq		xmm1,r11,0
	pinsrq		xmm1,rcx,1

	pxor		xmm2,xmm2
	pinsrq		xmm2,r11,0
	pinsrq		xmm2,r8,1

	pxor		xmm3,xmm3
	pinsrq		xmm3,r11,0
	pinsrq		xmm3,r9,1

	popq		r10
	pxor		xmm4,xmm4
	pinsrq		xmm4,r11,0
	pinsrq		xmm4,r10,1

	popq		r10
	pxor		xmm5,xmm5
	pinsrq		xmm5,r11,0
	pinsrq		xmm5,r10,1
	
t_no_sse:
	movq		rsp,rbp
	popq		rbp
	ret

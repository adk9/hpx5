
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

#include "hpx/thread/arch/x86_64/mconfig_defs.h"
	
.intel_syntax noprefix

.align 8
.globl HPX_CDECL(_fpu_crusher), HPX_CDECL(_sse_crusher)

.text
	
	
# --------------------------------------------------------------------
#  _fpu_crusher
#
#  Put some stuff to test in the FPU (if we have one).
# --------------------------------------------------------------------

HPX_CDECL(_fpu_crusher):
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


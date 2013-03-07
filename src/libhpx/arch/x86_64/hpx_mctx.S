
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#  Machine Context Switching Functions
# 
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

.intel_syntax
	
.globl _hpx_mctx_makecontext
	
.text
_hpx_mctx_makecontext:
	push	rbp
	mov	rbp,rsp

	# clear out some stuff in the struct
	movq	rcx,3
	xor	rax,rax
	rep	stosq	

	xor	rax,rax
	mov	rsp,rbp
	pop	rbp
	ret

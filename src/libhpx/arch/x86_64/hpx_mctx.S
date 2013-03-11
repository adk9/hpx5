
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  Machine Context Switching Functions# 
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

.align 16
.globl _hpx_mctx_makecontext, _hpx_mctx_getcontext

	
.text

# --------------------------------------------------------------------
#  hpx_mctx_getcontext
#
#  A replacement for the deprecated POSIX getcontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_getcontext:
	# save function call arguments
	leaq	rax,[rdi]
	movq	[rax],rdi
	movq	[rax+8],rsi
	movq	[rax+16],rdx
	movq	[rax+24],rcx
	movq	[rax+32],r8
	movq	[rax+40],r9

	# save other registers we want to preserve
	movq	[rax+48],rdx
	movq	[rax+56],rbp
	movq	[rax+64],r12
	movq	[rax+72],r13
	movq	[rax+80],r14
	movq	[rax+88],r15

	# save the FPU control state & registers using FXSAVE if
	# it is available.  Otherwise, just save the control state
	# and MXCSR register
	leaq	r10,[rsi+4]				# flags2
	leaq	r11,[rax+96]
	mov	ecx,[r10]
	
	bts	ecx,24
	jc	use_fxsave

	fnstenv	[r11]
	fldenv	[r11]
	stmxcsr	[rax+120]

use_fxsave:	
	fxsave	[r11]
	fnclex
	
	# save the function return address
	movq	r10,[rsp]
	movq	[rax+608],r10

	# save the stack pointer
	leaq	r10,[rsp+8]
	movq	[rdi+616],r10

	
	
	xor	rax,rax
	ret


# --------------------------------------------------------------------
#  hpx_mctx_makecontext
#
#  A replacement for the deprecated POSIX makecontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_makecontext:
	push	rbp
	mov	rbp,rsp

	xor	rax,rax
	mov	rsp,rbp
	pop	rbp
	ret

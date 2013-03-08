
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
	
.globl _hpx_mctx_makecontext, _hpx_mctx_getcontext
	
.text

# --------------------------------------------------------------------
#  hpx_mctx_getcontext
#
#  A replacement for the deprecated POSIX getcontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_getcontext:
	# save function call arguments
	movq	[rdi],rdi
	movq	[rdi+8],rsi
	movq	[rdi+16],rdx
	movq	[rdi+24],rcx
	movq	[rdi+32],r8
	movq	[rdi+40],r9

	# save other registers we want to preserve
	movq	[rdi+48],rdx
	movq	[rdi+56],rbp
	movq	[rdi+64],r12
	movq	[rdi+72],r13
	movq	[rdi+80],r14
	movq	[rdi+88],r15
	
	# save the function return address
	movq	rcx,[rsp]
	movq	[rdi+96],rcx

	# save floating point registers
	lea	rcx,[rdi+104]
	fnstenv [rcx]
	fldenv 	[rcx]
	stmxcsr	[rdi+120]
	
	# save the stack pointer
	lea	rcx,[rsp+8]
	movq	[rdi+128],rcx
	
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

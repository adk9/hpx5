
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

#ifdef __APPLE__
  #include <sys/syscall.h>
#endif
	
.globl _hpx_mctx_getcontext,_hpx_mctx_setcontext,_hpx_mctx,makecontext

	
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
	movq	[rax+48],rbx
	movq	[rax+56],rbp
	movq	[rax+64],r12
	movq	[rax+72],r13
	movq	[rax+80],r14
	movq	[rax+88],r15

	# save the FPU control state & registers using FXSAVE
	leaq	r10,[rax+96]
	fxsave	[r10]
	fnclex

	# save the function return address
	movq	r10,[rsp]
	movq	[rax+608],r10

	# save the stack pointer
	leaq	r10,[rsp+8]	
	movq	[rax+616],r10

	# save the set of blocked signals
	xchgq	rax,r11
	
#ifdef __APPLE__
	movq	rax,SYS___pthread_sigmask
	mov	rdi,1		                          # SIG_BLOCK
	movq	rsi,[r11+624]
	xor	rdx,rdx
	syscall
#endif
	
	xor	rax,rax
	ret



# --------------------------------------------------------------------
#  hpx_mctx_setcontext
#
#  A replacement for the deprecated POSIX setcontext() function.
# --------------------------------------------------------------------

_hpx_mctx_setcontext:
	movq	r9,rsi
	movq	r10,rdi
	
	# restore the set of blocked signals
#ifdef __APPLE__
	leaq	rcx,[rdi]
	movq	rax,SYS___pthread_sigmask
	mov	rdi,1
	leaq	rsi,[rcx+624]
	xor	rdx,rdx
	syscall
#endif

	xchgq	rdi,r10
	xchgq	rsi,r9
	leaq	rax,[rdi]
	
	# restore the FPU control state & registers with FXSAVE
	leaq	r10,[rax+96]
	fxrstor	[r10]
	fwait

	# restore the stack and non-volatile registers
	movq	rsp,[rax+616]
	movq	rbx,[rax+48]
	movq	rbp,[rax+56]
	movq	r12,[rax+64]
	movq	r13,[rax+72]
	movq	r14,[rax+80]
	movq	r15,[rax+88]

	# restore the return function address
	movq	rcx,[rax+608]
	pushq	rcx

	# restore the function call registers
	movq	rsi,[rax+8]
	movq	rdx,[rax+16]
	movq	rcx,[rax+24]
	movq	r8,[rax+32]
	movq	r9,[rax+40]

	movq	rdi,[rax]
	
	xor	rax,rax
	ret
	
	
# --------------------------------------------------------------------
#  hpx_mctx_makecontext
#
#  A replacement for the deprecated POSIX makecontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_makecontext:
	xor	rax,rax
	ret

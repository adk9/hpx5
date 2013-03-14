
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

#include "arch/x86_64/hpx_mconfig_defs.h"
	
.intel_syntax

#ifdef __APPLE__
  #include <sys/syscall.h>
#endif
	
.globl _hpx_mctx_getcontext,_hpx_mctx_setcontext,_hpx_mctx_makecontext

	
.text

# --------------------------------------------------------------------
#  hpx_mctx_getcontext
#
#  A replacement for the deprecated POSIX getcontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_getcontext:
	# save function call arguments
	leaq		rax,[rdi]
	movq		[rax+_HPX_MCTX_O_RDI],rdi
	movq		[rax+_HPX_MCTX_O_RSI],rsi
	movq		[rax+_HPX_MCTX_O_RDX],rdx
	movq		[rax+_HPX_MCTX_O_RCX],rcx
	movq		[rax+_HPX_MCTX_O_R8],r8
	movq		[rax+_HPX_MCTX_O_R9],r9

	# save other registers we want to preserve
	movq		[rax+_HPX_MCTX_O_RBX],rbx
	movq		[rax+_HPX_MCTX_O_RBP],rbp
	movq		[rax+_HPX_MCTX_O_R12],r12
	movq		[rax+_HPX_MCTX_O_R13],r13
	movq		[rax+_HPX_MCTX_O_R14],r14
	movq		[rax+_HPX_MCTX_O_R15],r15

	# save the FPU iff the extended switching state flag is set
	test		rdx,0x0000000000000001
	jz		g_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]
	
	# save the FPU control state & registers using FXSAVE
	fxsave		[r10]
	fnclex

	# save the function return address
g_no_extstate:	
	movq		r10,[rsp]
	movq		[rax+_HPX_MCTX_O_RIP],r10

	# save the stack pointer
	leaq		r10,[rsp+8]	
	movq		[rax+_HPX_MCTX_O_RSP],r10

	# save the set of blocked signals
	test		rdx,0x0000000000000002
	jz		g_no_signals
	xchgq		rax,r11
	
#ifdef __APPLE__
	movq		rax,SYS___pthread_sigmask
	mov		rdi,_HPX_MCTX_SIG_BLOCK
	movq		rsi,[r11+_HPX_MCTX_O_SIGMASK]
	xor		rdx,rdx
	syscall
#endif
g_no_signals:	
	xor		rax,rax
	ret



# --------------------------------------------------------------------
#  hpx_mctx_setcontext
#
#  A replacement for the deprecated POSIX setcontext() function.
# --------------------------------------------------------------------

_hpx_mctx_setcontext:
	# restore the set of blocked signals
	test		rdx,0x0000000000000002
	jz		s_no_signals
	
	movq		r9,rdi
	movq		r10,rsi
	movq		r11,rdx
	
#ifdef __APPLE__
	leaq		rcx,[rdi]
	movq		rax,SYS___pthread_sigmask
	mov		rdi,_HPX_MCTX_SIG_BLOCK
	leaq		rsi,[rcx+_HPX_MCTX_O_SIGMASK]
	xor		rdx,rdx
	syscall
#endif

	xchgq		rdx,r11
	xchgq		rsi,r10
	xchgq		rdi,r9

s_no_signals:	
	leaq		rax,[rdi]

	# restore the FPU state iff the extended switching state flag is set
	test		rdx,0x0000000000000001
	jz		s_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]
	
	# restore the FPU control state & registers with FXSAVE
	fxrstor		[r10]
	fwait

	# restore the stack and non-volatile registers
s_no_extstate:	
	movq		rsp,[rax+_HPX_MCTX_O_RSP]
	movq		rbx,[rax+_HPX_MCTX_O_RBX]
	movq		rbp,[rax+_HPX_MCTX_O_RBP]
	movq		r12,[rax+_HPX_MCTX_O_R12]
	movq		r13,[rax+_HPX_MCTX_O_R13]
	movq		r14,[rax+_HPX_MCTX_O_R14]
	movq		r15,[rax+_HPX_MCTX_O_R15]

	# restore the return function address
	movq		rcx,[rax+_HPX_MCTX_O_RIP]
	pushq		rcx

	# restore the function call registers
	movq		rsi,[rax+_HPX_MCTX_O_RSI]
	movq		rdx,[rax+_HPX_MCTX_O_RDX]
	movq		rcx,[rax+_HPX_MCTX_O_RCX]
	movq		r8,[rax+_HPX_MCTX_O_R8]
	movq		r9,[rax+_HPX_MCTX_O_R9]

	movq		rdi,[rax+_HPX_MCTX_O_RDI]
	
	xor		rax,rax
	ret
	
	
# --------------------------------------------------------------------
#  hpx_mctx_makecontext
#
#  A replacement for the deprecated POSIX makecontext() function.
# --------------------------------------------------------------------
	
_hpx_mctx_makecontext:
	leaq		rax,[rdi]

	# set up the new stack
#	movq		r10,[rax+_HPX_MCTX_O_SP]
#	movq		r11,[rax+_HPX_MCTX_O_SS]
#	addq		r10,r11
	
	xor		rax,rax
	ret


# --------------------------------------------------------------------
#  ___hpx_mctx_trampoline
#
#  Trampoline function for hpx_mctx_makecontext
# --------------------------------------------------------------------

___hpx_mctx_trampoline:
		
	
	xor		rax,rax
	ret
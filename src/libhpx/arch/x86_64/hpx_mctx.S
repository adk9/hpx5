
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  Machine Context Switching Functions
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
	
.intel_syntax noprefix

#ifdef __APPLE__
#include <sys/syscall.h>
#elif __linux__
#include <sys/syscall.h>
#endif
	
	
.align 8
.globl HPX_CDECL(hpx_mctx_getcontext),HPX_CDECL(hpx_mctx_setcontext),HPX_CDECL(hpx_mctx_swapcontext),HPX_CDECL(_hpx_mctx_trampoline)

#ifdef __linux__
.type  HPX_CDECL(hpx_mctx_getcontext), %function
.type  HPX_CDECL(hpx_mctx_setcontext), %function
.type  HPX_CDECL(hpx_mctx_swapcontext), %function
#endif
	
.text
	
	
# --------------------------------------------------------------------
#  _hpx_mctx_trampoline
#
#  Trampoline function for hpx_mctx_makecontext_va
# --------------------------------------------------------------------

HPX_CDECL(_hpx_mctx_trampoline):
	# hpx_mctx_makecontext_va stored our stack pointer in RBX
	movq		rsp,rbx

	# arguments to hpx_mctx_setcontext got pushed on the stack
	popq		r10
	popq		rdx
	popq		rsi
	popq		rdi

	# since we used RBX to pass our stack pointer, we have
	# to restore it.  at least under Darwin.
	leaq		rax,[rdi]
	movq		[rax+_HPX_MCTX_O_RBX],r10

	# make sure we have a context to switch to.  this could
	# be zero because the caller didn't set a link to a
	# previous context
	testq		rdi,rdi
	je		tr_procexit
	
	jmp		tr_setcontext_local

	# exit the process if mctx->link is NULL, with exit(0)
tr_procexit:
#ifdef __APPLE__
	movq		rax,_HPX_MACH_SYSCALL_CLASS_ID
	addq		rax,SYS_exit
	xor		rdi,rdi
	syscall
#endif

	# we should never get here.  if we do, burn it all down
	hlt

	
# --------------------------------------------------------------------
#  hpx_mctx_getcontext
#
#  A replacement for the deprecated POSIX getcontext() function.
# --------------------------------------------------------------------

HPX_CDECL(hpx_mctx_getcontext):
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
	test		rdx,1
	jz		g_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]
	
	# save the FPU control state & registers using FXSAVE
	test		rsi,_HPX_MCONFIG_HAS_FXSR
	jz		g_no_fxsave
	
	fxsave		[r10]
	fnclex
	jmp		g_fxsave_done
	
g_no_fxsave:
	# save the FPU control state & registers using FSAVE
	fnsave		[r10]
	frstor		[r10]
	fwait
	
g_fxsave_done:	
g_no_extstate:
	# save the function return address
	movq		r10,[rsp]
	movq		[rax+_HPX_MCTX_O_RIP],r10

	# save the stack pointer
	leaq		r10,[rsp+8]
	movq		[rax+_HPX_MCTX_O_RSP],r10

	# save the set of blocked signals
	test		rdx,2
	jz		g_no_signals

	# evidently BSD has different "classes" of syscalls.
	# we only use the bottom 32 bits of RAX to pass the index.
	#
	# the syscall class occupies the upper 16 bits of the dword
	# while the actual syscall index number lives in the lower
	# 16 bits.
	#
	# the Mach syscall class is defined in
	# _HPX_MACH_SYSCALL_CLASS_ID
	#
	# also, we set up a small stack frame here because we
	# don't already have one.  we don't have one because
	# having a stack frame might screw up the registers
	# we're trying to save/restore.  Like RSP, RBP, and
	# RBX (which is used as a PIC register in BSD/Darwin).
	#
	# on Linux, we use SYS_rt_sigprocmask instead of a
	# call to pthread_sigmask.  It requires a 4th argument,
	# which is the size of the signal mask.  It's passed
	# via R10, not RCX (as on Darwin).
	enter		32,0
	movq		rdi,_HPX_MCTX_SIG_BLOCK
	xor		rsi,rsi
	leaq		rdx,[rax+_HPX_MCTX_O_SIGMASK]
#ifdef __APPLE__
	movq		rax,_HPX_MACH_SYSCALL_CLASS_ID
	addq		rax,SYS___pthread_sigmask
#elif __linux__
	movq		rax,SYS_rt_sigprocmask
	movq		r10,8
#endif
	syscall
	leave
	
g_no_signals:	
	xor		rax,rax
	ret



# --------------------------------------------------------------------
#  hpx_mctx_setcontext
#
#  A replacement for the deprecated POSIX setcontext() function.
# --------------------------------------------------------------------

HPX_CDECL(hpx_mctx_setcontext):
tr_setcontext_local:	
	leaq		rax,[rdi]

	# restore the set of blocked signals
	test		rdx,2
	jz		s_no_signals
	
	movq		r8,rdi
	movq		r9,rsi
	movq		r11,rdx

	enter		32,0
	mov		rdi,_HPX_MCTX_SIG_SETMASK
	leaq		rsi,[rax+_HPX_MCTX_O_SIGMASK]
	xor		rdx,rdx
#ifdef __APPLE__
	leaq		rcx,[rdi]
	movq		rax,_HPX_MACH_SYSCALL_CLASS_ID
	addq		rax,SYS___pthread_sigmask
#elif __linux__
	leaq		r11,[rdi]
	movq		rax,SYS_rt_sigprocmask
	movq		r10,8
#endif
	syscall
	leave

	xchgq		rdx,r11
	xchgq		rsi,r9
	xchgq		rdi,r8

s_no_signals:	
	leaq		rax,[rdi]

	# restore the FPU state iff the extended switching state flag is set
	test		rdx,1
	jz		s_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]

	# restore the FPU state using FXRSTOR if supported
	test		rsi,2
	jz		s_no_fxrstor

	fxrstor		[r10]
	fwait
	jmp		s_fxrstor_done

s_no_fxrstor:
	# restore the FPU state using FRSTOR
	frstor		[r10]
	fwait

s_fxrstor_done:	
s_no_extstate:	
	# restore the stack and non-volatile registers
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
#  hpx_mctx_swapcontext
#
#  A replacement for the deprecated POSIX swapcontext() function.  
# --------------------------------------------------------------------

HPX_CDECL(hpx_mctx_swapcontext):
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
	test		rcx,1
	jz		swg_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]
	
	# save the FPU control state & registers using FXSAVE
	test		rdx,_HPX_MCONFIG_HAS_FXSR
	jz		swg_no_fxsave
	
	fxsave		[r10]
	fnclex
	jmp		swg_fxsave_done
	
swg_no_fxsave:
	# save the FPU control state & registers using FSAVE
	fnsave		[r10]
	frstor		[r10]
	
swg_fxsave_done:	
swg_no_extstate:
	# save the function return address
	movq		r10,[rsp]
	movq		[rax+_HPX_MCTX_O_RIP],r10

	# save the stack pointer
	leaq		r10,[rsp+8]
	movq		[rax+_HPX_MCTX_O_RSP],r10

	# save the set of blocked signals if we're supposed to
	test		rcx,2
	jz		swg_no_signals

	# preserve registers that are clobbered by the syscall
	# note that we don't care about RDI after this
	movq		r8,rsi
	movq		r9,rdx
	movq		r11,rcx
	
	# save the current signal mask
	enter		32,0
	mov		rdi,_HPX_MCTX_SIG_BLOCK
	xor		rsi,rsi
	leaq		rdx,[rax+_HPX_MCTX_O_SIGMASK]
#ifdef __APPLE__
	movq		rax,_HPX_MACH_SYSCALL_CLASS_ID
	addq		rax,SYS___pthread_sigmask
	syscall
#elif __linux__
	movq		r10,8
	movq		rax,SYS_rt_sigprocmask
	syscall
#endif

	# set the new signal mask
	movq		rcx,[r8]
	mov		rdi,_HPX_MCTX_SIG_SETMASK
	leaq		rsi,[rcx+_HPX_MCTX_O_SIGMASK]
	xor		rdx,rdx

#ifdef __APPLE__
	movq		rax,_HPX_MACH_SYSCALL_CLASS_ID
	addq		rax,SYS___pthread_sigmask
#elif __linux__
	movq		rax,SYS_rt_sigprocmask
	movq		r10,8
#endif
	syscall
	leave
	
	# restore clobbered registers (that we care about)
	xchgq		rcx,r11
	xchgq		rdx,r9
	xchgq		rsi,r8

swg_no_signals:	
	leaq		rax,[rsi]

	# restore the FPU state iff the extended switching state flag is set
	test		rcx,1
	jz		sws_no_extstate
	leaq		r10,[rax+_HPX_MCTX_O_FPREGS]

	# restore the FPU state using FXRSTOR if supported
	test		rdx,2
	jz		sws_no_fxrstor

	fxrstor		[r10]
	fwait
	jmp		sws_fxrstor_done

sws_no_fxrstor:
	# restore the FPU state using FRSTOR
	frstor		[r10]
	fwait

sws_fxrstor_done:	
sws_no_extstate:	
	# restore the stack and non-volatile registers
	movq		rsp,[rax+_HPX_MCTX_O_RSP]
	movq		rbx,[rax+_HPX_MCTX_O_RBX]
	movq		rbp,[rax+_HPX_MCTX_O_RBP]
	movq		r12,[rax+_HPX_MCTX_O_R12]
	movq		r13,[rax+_HPX_MCTX_O_R13]
	movq		r14,[rax+_HPX_MCTX_O_R14]
	movq		r15,[rax+_HPX_MCTX_O_R15]

	# restore the return function address
	movq		r10,[rax+_HPX_MCTX_O_RIP]
	pushq		r10

	# restore the function call registers
	movq		rsi,[rax+_HPX_MCTX_O_RSI]
	movq		rdx,[rax+_HPX_MCTX_O_RDX]
	movq		rcx,[rax+_HPX_MCTX_O_RCX]
	movq		r8,[rax+_HPX_MCTX_O_R8]
	movq		r9,[rax+_HPX_MCTX_O_R9]

	movq		rdi,[rax+_HPX_MCTX_O_RDI]

	xor		rax,rax
	ret

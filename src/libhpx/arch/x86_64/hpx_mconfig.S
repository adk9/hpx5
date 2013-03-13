
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

	
.intel_syntax

.globl _hpx_mconfig_init

	
.text

# --------------------------------------------------------------------
#  hpx_mconfig_init
#
#  Fetches machine config data.
# --------------------------------------------------------------------
	
_hpx_mconfig_init:
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
	
	# save the contents of ECX into flags1
	leaq	rax,[rdi]
	mov	[rax],ecx

	# save the contents of EDX into flags2
	mov	[rax+4],edx
	
	xor	rax,rax
	ret

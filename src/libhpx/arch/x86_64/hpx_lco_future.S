
# ==================================================================== 
#  High Performance ParalleX Library (libhpx)
#
#  LCO Functions - Futures
#  hpx_lco_future.S
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
	
	
.align 8
.globl HPX_CDECL(hpx_lco_future_init),HPX_CDECL(hpx_lco_future_set)

#ifdef __linux__
.type HPX_CDECL(hpx_lco_future_init), %function
.type HPX_CDECL(hpx_lco_future_set), %function
#endif
	
.text
	
	
# --------------------------------------------------------------------
#  hpx_lco_future_init
#
#  Initialize a future for future use (pun intended).
# --------------------------------------------------------------------

HPX_CDECL(hpx_lco_future_init):
	# set the future's state to UNSET
	leaq			r11,[rdi]
	xor			rax,rax
	mov			[r11],al

	# set the future's value to NULL
	movq			[r11+1],rax

	ret

	
# --------------------------------------------------------------------
#  hpx_lco_future_set
#
#  Atomically set a future.
# --------------------------------------------------------------------

HPX_CDECL(hpx_lco_future_set):
	# atomically set the future's state to SET
	mov			al,1
	xchg			[rdi],al
	
	ret
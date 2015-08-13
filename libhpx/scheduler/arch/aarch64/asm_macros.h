// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_ASM_MACROS_H
#define LIBHPX_ASM_MACROS_H

#if defined(HAVE_CFI_DIRECTIVES)
# define STARTPROC .cfi_startproc
# define ENDPROC .cfi_endproc
# define CFA_DEF_OFFSET(N) .cfi_def_cfa_offset N
# define CFA_ADJ_OFFSET(N) .cfi_adjust_cfa_offset N
# define CFA_REGISTER(R) .cfi_def_cfa_register R
# define CFA_DEF(R,N) .cfi_def_cfa R,N
# define CFI_RESTORE(R) .cfi_restore R
# define CFI_OFFSET(R,N) .cfi_offset R,N
# define CFI_REL_OFFSET(R,N) .cfi_rel_offset R,N
#else
# define STARTPROC
# define ENDPROC
# define CFA_DEF_OFFSET(N)
# define CFA_ADJ_OFFSET(N)
# define CFA_REGISTER(R)
# define CFA_DEF(R,N)
# define CFI_RESTORE(R)
# define CFI_OFFSET(R,N)
# define CFI_REL_OFFSET(R,N)
#endif


#if defined(__APPLE__)
#define GLOBAL(S) .globl _##S
#define LABEL(S) _##S:
#define INTERNAL(S) .private_extern _##S
#define SIZE(S)
#elif defined(__linux__)
#define GLOBAL(S) .global S
#define LABEL(S) S:
#define INTERNAL(S) .internal S
#define SIZE(S) .size S, .-S
#else
#error No ASM support for your platform.
#endif

#define GPR_LAYOUT			\
	REG_PAIR (x19, x20,  0);	\
	REG_PAIR (x21, x22, 16);	\
	REG_PAIR (x23, x24, 32);	\
	REG_PAIR (x25, x26, 48);	\
	REG_PAIR (x27, x28, 64);	\
	REG_PAIR (x29, x30, 80);

#define FPR_LAYOUT			\
	REG_PAIR ( d8,  d9, 96);	\
	REG_PAIR (d10, d11, 112);	\
	REG_PAIR (d12, d13, 128);	\
	REG_PAIR (d14, d15, 144);

#endif // LIBHPX_ASM_MACROS_H

// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
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

#if defined(__APPLE__)
# define GLOBAL(S) .globl _##S
# define LABEL(S) _##S:
# define INTERNAL(S) .private_extern _##S
# define SIZE(S)
# define TYPE(S, T) .type _##S, T
# define STARTPROC .cfi_startproc
# define ENDPROC .cfi_endproc
# define SECTION(S) .section .S
# define CFA_DEF_OFFSET(N) .cfi_def_cfa_offset N
# define CFA_ADJ_OFFSET(N) .cfi_adjust_cfa_offset N
# define CFA_REGISTER(R) .cfi_def_cfa_register R
# define CFA_DEF(R,N) .cfi_def_cfa R,N
# define CFI_RESTORE(R) .cfi_restore R
# define CFI_OFFSET(R,N) .cfi_offset R,N
# define CFI_REL_OFFSET(R,N) .cfi_rel_offset R,N
#elif defined(__linux__)
# define GLOBAL(S) .globl S
# define LABEL(S) S:
# define INTERNAL(S) .internal S
# define SIZE(S) .size S, .-S
# define TYPE(S, T) .type S, T
# define STARTPROC .cfi_startproc
# define ENDPROC .cfi_endproc
# define SECTION(S) .section .S
# define CFA_DEF_OFFSET(N) .cfi_def_cfa_offset N
# define CFA_ADJ_OFFSET(N) .cfi_adjust_cfa_offset N
# define CFA_REGISTER(R) .cfi_def_cfa_register R
# define CFA_DEF(R,N) .cfi_def_cfa R,N
# define CFI_RESTORE(R) .cfi_restore R
# define CFI_OFFSET(R,N) .cfi_offset R,N
# define CFI_REL_OFFSET(R,N) .cfi_rel_offset R,N
#else
# error No ASM support for your platform.
#endif

#endif // LIBHPX_ASM_MACROS_H

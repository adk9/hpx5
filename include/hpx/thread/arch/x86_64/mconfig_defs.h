/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Configuration Defines
  hpx_mconfig_defs.h

  Copyright (c) 2013,      Trustees of Indiana University 
  Copyright (c) 2002-2012, Intel Corporation
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_MCONFIG_DEFS_X86_64_H_
#define LIBHPX_MCONFIG_DEFS_X86_64_H_

#define _HPX_MCONFIG_HAS_FPU  0x0000000000000001  /* CPU has an x87 floating point unit */
#define _HPX_MCONFIG_HAS_FXSR 0x0000000000000002  /* CPU has an FXSR */
#define _HPX_MCONFIG_HAS_AVX  0x0000000000000004  /* CPU has some version of AVX */
#define _HPX_MCONFIG_HAS_TSC  0x0000000000000008  /* CPU has a time-stamp counter */
#define _HPX_MCONFIG_HAS_SSE  0x0000000000000010  /* CPU has some version of SSE */

/*
 --------------------------------------------------------------------
  Macros
 --------------------------------------------------------------------
*/

#ifdef __APPLE__
#define HPX_CDECL(label) _##label
#else
#define HPX_CDECL(label) label
#endif


/*
 --------------------------------------------------------------------
  SYSCALL Defines
 --------------------------------------------------------------------
*/

#ifdef __APPLE__
  #define _HPX_MACH_SYSCALL_CLASS_ID  0x2000000
  #define _HPX_MCTX_SIG_BLOCK         1
  #define _HPX_MCTX_SIG_SETMASK       3
#elif __linux__
  #define _HPX_MCTX_SIG_BLOCK         0
  #define _HPX_MCTX_SIG_SETMASK       2
#endif


/*
 --------------------------------------------------------------------
  Offsets
  
  TODO: figure out how to automagically calculate these
 --------------------------------------------------------------------
*/

#define _HPX_MCTX_O_RDI               0
#define _HPX_MCTX_O_RSI               8
#define _HPX_MCTX_O_RDX              16
#define _HPX_MCTX_O_RCX              24
#define _HPX_MCTX_O_R8               32
#define _HPX_MCTX_O_R9               40
#define _HPX_MCTX_O_RBX              48
#define _HPX_MCTX_O_RBP              56
#define _HPX_MCTX_O_R12              64
#define _HPX_MCTX_O_R13              72
#define _HPX_MCTX_O_R14              80
#define _HPX_MCTX_O_R15              88

#define _HPX_MCTX_O_FPREGS           96

#define _HPX_MCTX_O_FPREGS_FCW      104
#define _HPX_MCTX_O_FPREGS_FSW      106
#define _HPX_MCTX_O_FPREGS_FTW      108
#define _HPX_MCTX_O_FPREGS_PAD1     109
#define _HPX_MCTX_O_FPREGS_FOP      110
#define _HPX_MCTX_O_FPREGS_FPUIP    112
#define _HPX_MCTX_O_FPREGS_FPUDP    120

#define _HPX_MCTX_O_FPREGS_MXCSR    120

#define _HPX_MCTX_O_FPREGS_ST0      128
#define _HPX_MCTX_O_FPREGS_STRES0   138
#define _HPX_MCTX_O_FPREGS_ST1      144
#define _HPX_MCTX_O_FPREGS_STRES1   154
#define _HPX_MCTX_O_FPREGS_ST2      160
#define _HPX_MCTX_O_FPREGS_STRES2   170
#define _HPX_MCTX_O_FPREGS_ST3      176
#define _HPX_MCTX_O_FPREGS_STRES3   186
#define _HPX_MCTX_O_FPREGS_ST4      192
#define _HPX_MCTX_O_FPREGS_STRES4   202
#define _HPX_MCTX_O_FPREGS_ST5      208
#define _HPX_MCTX_O_FPREGS_STRES5   218
#define _HPX_MCTX_O_FPREGS_ST6      224
#define _HPX_MCTX_O_FPREGS_STRES6   234
#define _HPX_MCTX_O_FPREGS_ST7      240
#define _HPX_MCTX_O_FPREGS_STRES7   250

#define _HPX_MCTX_O_FPREGS_XMM0     256
#define _HPX_MCTX_O_FPREGS_XMM1     272
#define _HPX_MCTX_O_FPREGS_XMM2     288
#define _HPX_MCTX_O_FPREGS_XMM3     304
#define _HPX_MCTX_O_FPREGS_XMM4     320
#define _HPX_MCTX_O_FPREGS_XMM5     336
#define _HPX_MCTX_O_FPREGS_XMM6     352
#define _HPX_MCTX_O_FPREGS_XMM7     368
#define _HPX_MCTX_O_FPREGS_XMM8     384
#define _HPX_MCTX_O_FPREGS_XMM9     400
#define _HPX_MCTX_O_FPREGS_XMM10    416
#define _HPX_MCTX_O_FPREGS_XMM11    432
#define _HPX_MCTX_O_FPREGS_XMM12    448
#define _HPX_MCTX_O_FPREGS_XMM13    464
#define _HPX_MCTX_O_FPREGS_XMM14    480
#define _HPX_MCTX_O_FPREGS_XMM15    496

#define _HPX_MCTX_O_FPREGS_PAD4     512

#define _HPX_MCTX_O_RIP             608
#define _HPX_MCTX_O_RSP             616

#define _HPX_MCTX_O_SIGMASK         624

#define _HPX_MCTX_O_SP              632
#define _HPX_MCTX_O_SS              640

#define _HPX_MCTX_O_LINK            648

#endif /* LIBHPX_MCONFIG_DEFS_X86_64_H_ */


/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Context Register Table for x86_64 CPUs
  hpx_mregs.h

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
#ifndef LIBHPX_MREGS_X86_64_H_
#define LIBHPX_MREGS_X86_64_H_

#include <stdint.h>


/*
 --------------------------------------------------------------------
  Machine Register Tables
 --------------------------------------------------------------------
*/

/*
  modified from intel-fp.hpp at:
  https://svn.mcs.anl.gov/repos/performance/Gwalp/gwalpsite/pin/extras/components/include/util/intel-fp.hpp

  March 8, 2013  13:15 EST
*/

typedef union {
  struct {
    uint64_t significand;       /* the floating-point significand */
    uint16_t exponent;          /* the floating-point exponent */
    uint16_t pad[3];
  } _fp;

  struct {
    uint64_t lo;                /* least significant part of value */
    uint64_t hi;                /* most significant part of value */
  } _raw;
} hpx_x87reg_padded_t __attribute__((aligned (16)));

typedef union {
  uint8_t  vec8[16];
  uint16_t vec16[8];
  uint32_t vec32[4];
  uint64_t vec64[2];
} hpx_xmmreg_t __attribute__((aligned (16)));

typedef struct {
  uint16_t fcw;                /* X87 control word */
  uint16_t fsw;                /* X87 status word */
  uint8_t  ftw;                /* Abridged X87 tag value */
  uint8_t  pad1;
  uint16_t fop;                /* Last X87 non-control instruction opcode */
  uint64_t fpuip;              /* Last X87 non-control instruction address */
  uint64_t fpudp;              /* Last X87 non-control instruction operand segment offset */
  uint32_t mxcsr;              /* MXCSR control and status register */
  uint32_t mxcsrmask;          /* Mask of valid MXCSR bits */

  hpx_x87reg_padded_t sts[8];  /* X87 data registers in top-of-stack order */
  hpx_xmmreg_t xmms[16];       /* XMM registers */
  uint64_t pad4[12];

} hpx_fpregs_t __attribute__ ((aligned (16)));

typedef struct {
  /* function call parameter passing registers */
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t r8;
  uint64_t r9;

  /* other registers to preserve */
  uint64_t rbx;
  uint64_t rbp;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;

  /* floating point registers */
  hpx_fpregs_t fpregs;

  /* return function address */
  uint64_t rip;

  /* stack pointer */
  uint64_t rsp;
} hpx_mregs_t __attribute__((aligned (16)));

#endif

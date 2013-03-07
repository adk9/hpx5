
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Context Register Table for x86_64 CPUs
  hpx_mregs.h

  Copyright (c) 2013, Trustees of Indiana University 
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
  Machine Register Table
 --------------------------------------------------------------------
*/

typedef struct {
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t rcx;
  uint64_t r8;
  uint64_t r9;
} hpx_mregs_t;

#endif

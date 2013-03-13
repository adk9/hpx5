
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Context Switch Functions
  hpx_mctx.h

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
#ifndef LIBHPX_MCTX_H_
#define LIBHPX_MCTX_H_

#include <signal.h>
#include <stdint.h>

#ifdef __x86_64__
  #include "arch/x86_64/hpx_mconfig.h"
  #include "arch/x86_64/hpx_mregs.h"
#endif


/*
 --------------------------------------------------------------------
  Machine Context Data
 --------------------------------------------------------------------
*/

typedef void (*hpx_mctx_func_t)(void);

typedef struct {
  hpx_mregs_t regs;
  sigset_t sigs;
  uint64_t exec_t1;
  uint64_t exec_t2;
} hpx_mctx_context_t;


/*
 --------------------------------------------------------------------
  Machine Context Functions
 --------------------------------------------------------------------
*/

void hpx_mctx_getcontext(hpx_mctx_context_t *, hpx_mconfig_t *);
void hpx_mctx_makecontext(hpx_mctx_context_t *, hpx_mconfig_t *, void *, int, ...);

#endif

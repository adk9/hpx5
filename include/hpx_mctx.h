
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

#ifdef __x86_64__
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
  void * sp;
} hpx_mctx_context_t;


/*
 --------------------------------------------------------------------
  Machine Context Functions
 --------------------------------------------------------------------
*/

void hpx_mctx_getcontext(hpx_mctx_context_t *);
void hpx_mctx_makecontext(hpx_mctx_context_t *, void *, int, ...);

#endif

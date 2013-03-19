
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


/* save/restore FPU registers during context switches */
#define HPX_MCTX_SWITCH_EXTENDED                                0x01

/* save/restore per-thread signal masks during context switches */
#define HPX_MCTX_SWITCH_SIGNALS                                 0x02


/*
 --------------------------------------------------------------------
  Machine Context Data
 --------------------------------------------------------------------
*/

typedef void (*hpx_mctx_func_t)(void);

typedef struct _hpx_mctx_context_t {
  hpx_mregs_t regs;
  sigset_t sigs;
  void * sp;
  uint64_t ss;
  struct _hpx_mctx_context_t * link;
} hpx_mctx_context_t;


/*
 --------------------------------------------------------------------
  Machine Context Functions
 --------------------------------------------------------------------
*/

void hpx_mctx_getcontext(hpx_mctx_context_t *, hpx_mconfig_t, uint64_t);
void hpx_mctx_setcontext(hpx_mctx_context_t *, hpx_mconfig_t, uint64_t);
void hpx_mctx_makecontext(hpx_mctx_context_t *, hpx_mconfig_t, uint64_t, void *, int, ...);
void hpx_mctx_swapcontext(hpx_mctx_context_t *, hpx_mctx_context_t *, hpx_mconfig_t, uint64_t);

void _hpx_mctx_boot(void);

#endif

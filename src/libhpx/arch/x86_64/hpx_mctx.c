
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Machine Context Switch Functions for the x86_64 Architecture
  hpx_mctx_c.c

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


/*
 --------------------------------------------------------------------
  hpx_mctx_makecontext

  Creates a new context that calls a function before returning to
  a context created by hpx_mctx_getcontext().
 --------------------------------------------------------------------
*/

void hpx_mctx_makecontext(hpx_mcontext_t * mctx, hpx_mconfig_t mcfg, uint64_t mflags, void * func, int argc, ...) {
  void * sp;

  /* set the stack pointer */
  sp = (void *) mctx->sp + mctx->ss;
  sp -= 16;

  mctx->mregs.rsp = sp;
  mctx->mregs.rip = func;
  mctx->mregs.rbx = (sp + 8);
  
  sp[0] = &_hpx_mctx_boot;
  sp[1] = mctx->link;
}

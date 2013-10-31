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
#include <stdarg.h>
#include <stdint.h>

#ifdef __x86_64__
  #include "arch/x86_64/mconfig.h"
  #include "arch/x86_64/mregs.h"
#endif


/* save/restore FPU registers during context switches */
#define HPX_MCTX_SWITCH_EXTENDED  1

/* save/restore per-thread signal masks during context switches */
#define HPX_MCTX_SWITCH_SIGNALS   2


/*
 --------------------------------------------------------------------
  Machine Context Data
 --------------------------------------------------------------------
*/

struct hpx_mctx_context {
  hpx_mregs_t               regs;
  sigset_t                  sigs;
  void                       *sp;
  uint64_t                    ss;
  struct hpx_mctx_context  *link;
};

/*
 --------------------------------------------------------------------
  Machine Context Functions
 --------------------------------------------------------------------
*/

void hpx_mctx_getcontext(struct hpx_mctx_context *, hpx_mconfig_t, uint64_t);
void hpx_mctx_setcontext(struct hpx_mctx_context *, hpx_mconfig_t, uint64_t);
void hpx_mctx_makecontext(struct hpx_mctx_context *, struct hpx_mctx_context *, void *, size_t, hpx_mconfig_t, uint64_t, void (*)(), int, ...);
void hpx_mctx_makecontext_va(struct hpx_mctx_context *, struct hpx_mctx_context *, void *, size_t, hpx_mconfig_t, uint64_t, void (*)(), int, va_list *);
void hpx_mctx_swapcontext(struct hpx_mctx_context *, struct hpx_mctx_context *, hpx_mconfig_t, uint64_t);

#endif /* LIBHPX_MCTX_H_ */

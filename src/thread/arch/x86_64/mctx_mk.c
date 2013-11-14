/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Machine Context Switching Functions: makecontext() Replacement
  hpx_mctx_mk.c

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include "hpx/thread/mctx.h"                    /* struct hpx_mctx_context */
#include "hpx/system/attributes.h"              /* HPX_ATTRIBUTE */

extern void _hpx_mctx_trampoline(void)
  HPX_ATTRIBUTE(HPX_NORETURN);

/**
 * Replacement for the deprecated POSIX makecontext() function.
 *
 * We do this in C because we have to work with variadic arguments, which are
 * implemented by the compiler via macros.  They're cumbersome and error-prone
 * in assembly.
 */
void hpx_mctx_makecontext_va(hpx_mctx_context_t *mctx, hpx_mctx_context_t *link_mctx,
                             void *stk, size_t ss, hpx_mconfig_t mcfg, uint64_t mflags,
                             void (*func)(), int argc, va_list *argv) {
  uint64_t *sp;
  uint64_t *reg;
  int arg_cnt;
  int sp_cnt;
  int idx;

  /* set up our stack frame and make room for stuff */
  mctx->sp = stk;
  mctx->ss = ss;

  /* initialize our new machine context, if we have a link */
  if (link_mctx) {
    memcpy(mctx, link_mctx, sizeof(hpx_mctx_context_t));
  }

  sp = (uint64_t *) stk + (ss / sizeof(uint64_t));

  (argc > 6) ? (sp_cnt = (argc - 6)) : (sp_cnt = 0);
  sp -= (5 + sp_cnt);

  /* save our context linkage to the new stack */
  sp[4 + sp_cnt] = (uint64_t) link_mctx;
  mctx->link = link_mctx;

  /* save our machine configuration flags to the new stack */
  sp[3 + sp_cnt] = (uint64_t) mcfg;

  /* save our context switching flags to the new stack */
  sp[2 + sp_cnt] = mflags;

  /* save our original RBX register value to the new stack */
  sp[1 + sp_cnt] = mctx->regs.rbx;

  /* set the base pointer for the trampoline function */
  /* we pass this via RBX because we need to preserve the RBP we have saved now */
  mctx->regs.rbx = (uint64_t) &sp[1 + sp_cnt];

  /* save our trampoline function address to the new stack */
  sp[0] = (uint64_t) &_hpx_mctx_trampoline;

  /* set our new stack pointer */
  mctx->regs.rsp = (uint64_t) sp;

  /* set our new instruction pointer */
  mctx->regs.rip = (uint64_t) func;

  /* save arguments that are passed via registers */
  /* this assumes the function call registers are in order: */
  /* RDI, RSI, RDX, RCX, R8, R9 */
  (argc > 6) ? (arg_cnt = 6) : (arg_cnt = argc);
  reg = &mctx->regs.rdi;

  for (idx = 0; idx < arg_cnt; idx++) {
    reg[idx] = va_arg(*argv, uint64_t);
  }

  /* save arguments that are passed via the stack */
  for (idx = 1; idx <= sp_cnt; idx++) {
    sp[idx] = (uint64_t) va_arg(*argv, uint64_t);
  }
}


/**
 * Wrapper for hpx_mctx_makecontext_va.
 */
void hpx_mctx_makecontext(hpx_mctx_context_t *mctx, hpx_mctx_context_t *link_mctx, void *stk,
                          size_t ss, hpx_mconfig_t mcfg, uint64_t mflags, void (*func)(),
                          int argc, ...) {
  va_list argv;

  va_start(argv, argc);
  hpx_mctx_makecontext_va(mctx, link_mctx, stk, ss, mcfg, mflags, func, argc, &argv);
  va_end(argv);
}

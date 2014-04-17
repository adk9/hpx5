/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "backoff.h"
#include "nop.h"


/**
 *  "Magic" backoff constants.
 */
static const int multiplier = 2;
static const int limit = 65536;


/* Modern compilers are smart. No need to macro this. */
static int min(int x, int y) {
  return (x < y) ? x : y;
}

/*  Backoff for now just does some wasted work. Make sure that this is
 *  not optimized.
 */
void backoff(int *prev) {
  *prev = min(*prev * multiplier, limit);
  for (int i = 0, e = *prev; i < e; ++i)
    sync_nop();
}


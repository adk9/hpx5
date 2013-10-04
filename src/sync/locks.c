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

#include "locks.h"

/*
  ====================================================================
  Backoff for now just does some wasted work. Make sure that this is
  not optimized.
  ====================================================================
*/
static  __attribute__((noinline))
void backoff(int *prev) {
    int i, e;
    for (i = 0, e = *prev * 2; i < e; ++i)
        *prev += 1;
}

void hpx_sync_tatas_acquire_slow(tatas_t *l) {
    int i = 16;
    do {
        backoff(&i);
    } while (hpx_sync_swap(&l->lock, 1, HPX_SYNC_ACQUIRE));
}

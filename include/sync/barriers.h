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

#ifndef HPX_SYNC_BARRIERS_H_
#define HPX_SYNC_BARRIERS_H_

typedef struct sr_barrier sr_barrier_t;

sr_barrier_t *sr_barrier_create(int n_threads);
void sr_barrier_destroy(sr_barrier_t *barrier);
void sr_barrier_join(sr_barrier_t *barrier, int thread_id);

#endif /* HPX_SYNC_BARRIERS_H_ */

// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef PQUEUE_WRAPPER_H
#define PQUEUE_WRAPPER_H

#include <stdio.h>
#include "pqueue.h"
#include "libsync/locks.h"

typedef struct {
  pqueue_t *pq;
  tatas_lock_t mutex; //mutex associated with the priority queue
} locked_pq;

typedef locked_pq sssp_queue_t;

#endif // PQUEUE_WRAPPER_H

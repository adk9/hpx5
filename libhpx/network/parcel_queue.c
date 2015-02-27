// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "libhpx/debug.h"
#include "libhpx/parcel.h"

void parcel_queue_init(parcel_queue_t *q) {
  q->head = NULL;
  q->tail = NULL;
}


void parcel_queue_fini(parcel_queue_t *q) {
  if (q->head) {
    dbg_error("parcels remain in queue\n");
  }
}


void parcel_queue_enqueue(parcel_queue_t *q, hpx_parcel_t *p) {
  DEBUG_IF (parcel_get_stack(p) != NULL) {
    dbg_error("cannot enqueue a parcel with an active stack");
  }

  if (!q->head) {
    q->head = p;
  }

  if (q->tail) {
    parcel_set_stack(q->tail, (void*)p);
  }

  q->tail = p;
}


hpx_parcel_t *parcel_queue_dequeue(parcel_queue_t *q) {
  hpx_parcel_t *p = q->head;
  if (p) {
    q->head = (void*)parcel_get_stack(p);
    parcel_set_stack(p, NULL);
  }
  if (p == q->tail) {
    q->tail = NULL;
  }
  return p;
}


hpx_parcel_t *parcel_queue_dequeue_all(parcel_queue_t *q) {
  hpx_parcel_t *p = q->head;
  q->head = NULL;
  q->tail = NULL;
  return p;
}


/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  "Kernel" Thread Function Definitions
  hpx_kthread.h

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
#ifndef LIBHPX_KTHREAD_H_
#define LIBHPX_KTHREAD_H_

#include <stdint.h>
#include <pthread.h>
#include "hpx/types.h"
#include "hpx/thread/mctx.h"
#include "hpx/utils/queue.h"

/*
 --------------------------------------------------------------------
  Kernel Thread States
 --------------------------------------------------------------------
*/

#define HPX_KTHREAD_STATE_STOPPED   0
#define HPX_KTHREAD_STATE_RUNNING   1
#define HPX_KTHREAD_STATE_BUSY      2

/*
 --------------------------------------------------------------------
  Kernel Thread Data
 --------------------------------------------------------------------
*/

typedef pthread_mutex_t hpx_kthread_mutex_t;

struct hpx_kthread_t {
  hpx_kthread_mutex_t     mtx;
  pthread_cond_t          k_c;
  pthread_t               core_th;
  hpx_queue_t             pend_q;
  hpx_queue_t             susp_q;
  hpx_thread_t           *exec_th;
  hpx_context_t          *ctx;
  uint8_t                 k_st;
  hpx_mctx_context_t     *mctx;
  hpx_mconfig_t           mcfg;
  uint64_t                mflags;
  uint64_t                pend_load;
  uint64_t                wait_load;
};

typedef void *(*hpx_kthread_seed_t)(void *);

/*
 --------------------------------------------------------------------
  Seed Function
 --------------------------------------------------------------------
*/
void * hpx_kthread_seed_default(void *);


/*
 --------------------------------------------------------------------
  Kernel Thread Functions
 --------------------------------------------------------------------
*/
hpx_kthread_t * hpx_kthread_create(hpx_context_t *, hpx_kthread_seed_t, hpx_mconfig_t, uint64_t);
void hpx_kthread_set_affinity(hpx_kthread_t *, uint16_t);
void hpx_kthread_destroy(hpx_kthread_t *);

hpx_kthread_t * hpx_kthread_self(void);

void hpx_kthread_mutex_init(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_lock(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_unlock(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_destroy(hpx_kthread_mutex_t *);

/*
 --------------------------------------------------------------------
  Support Functions
 --------------------------------------------------------------------
*/
long hpx_kthread_get_cores(void);

#endif /* LIBHPX_KTHREAD_H */

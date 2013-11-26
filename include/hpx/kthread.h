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
#include "hpx/thread/arch/mconfig.h"
#include "hpx/utils/queue.h"

typedef struct hpx_kthread hpx_kthread_t;
typedef pthread_mutex_t hpx_kthread_mutex_t;
typedef void *(*hpx_kthread_seed_t)(void*);

struct hpx_context;
struct hpx_mctx_context;
struct hpx_thread;

enum hpx_kthread_state {
  HPX_KTHREAD_STATE_STOPPED = 0,
  HPX_KTHREAD_STATE_RUNNING = 1,
  HPX_KTHREAD_STATE_BUSY    = 2
};

struct hpx_kthread {
  hpx_kthread_mutex_t       mtx;                /*!< */
  pthread_cond_t            k_c;                /*!< */
  pthread_t             core_th;                /*!< */
  hpx_queue_t            pend_q;                /*!< */
  hpx_queue_t            susp_q;                /*!< */
  struct hpx_thread    *exec_th;                /*!< */
  struct hpx_context       *ctx;                /*!< */
  uint8_t                  k_st;                /*!< */
  struct hpx_mctx_context *mctx;                /*!< */
  hpx_mconfig_t            mcfg;                /*!< */
  uint64_t               mflags;                /*!< */
  uint64_t            pend_load;                /*!< */
  uint64_t            wait_load;                /*!< */
  int                       tid;                /*!< 0-based, dense thread id  */
};


void *hpx_kthread_seed_default(void *);

hpx_kthread_t *hpx_kthread_create(struct hpx_context *, hpx_kthread_seed_t,
                                  hpx_mconfig_t, uint64_t, int tid);
void hpx_kthread_set_affinity(hpx_kthread_t *, uint16_t);
void hpx_kthread_destroy(hpx_kthread_t *);
hpx_kthread_t *hpx_kthread_self(void);

/**
 * kthread mutex interface
 * @{
 */
void hpx_kthread_mutex_init(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_lock(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_unlock(hpx_kthread_mutex_t *);
void hpx_kthread_mutex_destroy(hpx_kthread_mutex_t *);
/** @} */
   

/*
 --------------------------------------------------------------------
  Support Functions
 --------------------------------------------------------------------
*/
long hpx_kthread_get_cores(void);

#endif /* LIBHPX_KTHREAD_H */

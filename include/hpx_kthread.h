
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

#include <stdint.h>
#include <pthread.h>
#include "hpx_queue.h"
#include "hpx_mctx.h"

#pragma once
#ifndef LIBHPX_KTHREAD_H_
#define LIBHPX_KTHREAD_H_

struct _hpx_thread_t;

static pthread_once_t __kthread_init_once = PTHREAD_ONCE_INIT;
static pthread_key_t errno_key;
static pthread_key_t kth_key;


/*
 --------------------------------------------------------------------
  Kernel Thread States
 --------------------------------------------------------------------
*/

#define HPX_KTHREAD_STATE_STOPPED                                  0
#define HPX_KTHREAD_STATE_RUNNING                                  1
#define HPX_KTHREAD_STATE_BUSY                                     2


/*
 --------------------------------------------------------------------
  Kernel Thread Data
 --------------------------------------------------------------------
*/

typedef struct _hpx_kthread_t {
  pthread_mutex_t mtx;
  pthread_cond_t k_c;
  pthread_t core_th;
  hpx_queue_t pend_q;
  struct _hpx_thread_t * exec_th;
  uint8_t k_st;
  hpx_mctx_context_t * mctx;
  hpx_mconfig_t mcfg;
  uint64_t mflags;
} hpx_kthread_t;

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

hpx_kthread_t * hpx_kthread_create(hpx_kthread_seed_t, hpx_mconfig_t, uint64_t);
uint16_t hpx_kthread_get_affinity(hpx_kthread_t *);
void hpx_kthread_set_affinity(hpx_kthread_t *, uint16_t);
void hpx_kthread_destroy(hpx_kthread_t *);

void _hpx_kthread_sched(hpx_kthread_t *, struct _hpx_thread_t *);
void _hpx_kthread_push_pending(hpx_kthread_t *, struct _hpx_thread_t *);

void _hpx_kthread_init(void);
static void __hpx_kthread_make_keys(void);

hpx_kthread_t * hpx_kthread_self(void);


/*
 --------------------------------------------------------------------
  Support Functions
 --------------------------------------------------------------------
*/

long hpx_kthread_get_cores(void);

#endif

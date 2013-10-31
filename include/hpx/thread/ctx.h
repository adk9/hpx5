/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Scheduling Context Function Definitions
  hpx_ctx.h

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
#ifndef HPX_CONTEXT_H_
#define HPX_CONTEXT_H_

#include <stdint.h>

#include "hpx/lco.h"                            /* struct hpx_future */
#include "hpx/thread.h"
#include "hpx/kthread.h"
#include "hpx/config.h"
#include "hpx/types.h"
#include "hpx/utils/queue.h"

#ifdef __x86_64__
  #include "arch/x86_64/mconfig.h"
#else
  typedef hpx_mconfig_t uint64_t;
#endif

/*
 --------------------------------------------------------------------
  Context Data
 --------------------------------------------------------------------
*/

/* the context ID type */
typedef uint64_t hpx_context_id_t;

/* the context handle */
struct hpx_context {
  hpx_context_id_t    cid;
  hpx_kthread_t     **kths;
  uint32_t            kths_count;
  uint32_t            kths_idx;

  hpx_kthread_mutex_t mtx;

  hpx_config_t        cfg;
  hpx_mconfig_t       mcfg;
  uint64_t            mflags;

  hpx_queue_t         term_ths;
  hpx_queue_t         term_stks;
  hpx_queue_t         term_lcos;

  /* service threads */
  hpx_thread_t      **srv_susp;
  hpx_thread_t       *srv_rebal;

  hpx_future_t        f_srv_susp;
  hpx_future_t        f_srv_rebal;

  //  hwloc_topology_t hw_topo;
};


/*
 --------------------------------------------------------------------
  Context Functions
 --------------------------------------------------------------------
*/

hpx_context_t *hpx_ctx_create(hpx_config_t *);
void hpx_ctx_destroy(hpx_context_t *);
hpx_context_id_t hpx_ctx_get_id(hpx_context_t *);

#endif /* HPX_CONTEXT_H_ */

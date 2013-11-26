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
#include "hpx/config.h"                         /* hpx_config_t */
#include "hpx/lco.h"                            /* struct hpx_future */
#include "hpx/kthread.h"                        /* hpx_kthread_mutex_t */
#include "hpx/thread/arch/mconfig.h"            /* hpx_mconfig_t */
#include "hpx/utils/queue.h"

struct sr_barrier;
typedef struct hpx_context hpx_context_t;
typedef uint64_t hpx_context_id_t;

struct hpx_thread;

/**
 * the context handle
 * @todo what is a context?
 */
struct hpx_context {
  hpx_context_id_t          cid;                /*!<  */
  hpx_kthread_t          **kths;                /*!<  */
  uint32_t           kths_count;                /*!<  */
  uint32_t             kths_idx;                /*!<  */

  hpx_kthread_mutex_t       mtx;                /*!<  */

  hpx_config_t              cfg;                /*!<  */
  hpx_mconfig_t            mcfg;                /*!<  */
  uint64_t               mflags;                /*!<  */

  hpx_queue_t          term_ths;                /*!<  */
  hpx_queue_t         term_stks;                /*!<  */
  hpx_queue_t         term_lcos;                /*!<  */

  /* service threads */
  struct hpx_thread  **srv_susp;                /*!<  */
  struct hpx_thread  *srv_rebal;                /*!<  */

  hpx_future_t       f_srv_susp;                /*!<  */
  hpx_future_t      f_srv_rebal;                /*!<  */

  struct sr_barrier    *barrier;                /*!< an sr barrier */
};

hpx_context_t *hpx_ctx_create(hpx_config_t *context);
void hpx_ctx_destroy(hpx_context_t *);
hpx_context_id_t hpx_ctx_get_id(hpx_context_t *);

#endif /* HPX_CONTEXT_H_ */

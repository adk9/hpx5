/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Scheduling Context Function Definitions
  hpx/ctx.h

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
#include "hpx/system/attributes.h"
#include "hpx/thread/arch/mconfig.h"            /* hpx_mconfig_t */
#include "hpx/utils/queue.h"
#include "sync/locks.h"

struct sr_barrier;
typedef struct hpx_context hpx_context_t;
typedef uint64_t hpx_context_id_t;

struct hpx_thread;

struct HPX_INTERNAL callback_list_node;
typedef struct callback_list_node callback_list_node_t;

/**
 * A callback list. Single lock synchronization should be fine since it is only
 * needed for append an free_all. It's probably overkill in fact---one thread
 * does all of the appending and free-ing at this point, however it's better to
 * be safe than sorry.
 */
struct HPX_INTERNAL callback_list;
typedef struct callback_list callback_list_t;

struct callback_list {
  tatas_lock_t lock;
  callback_list_node_t *head;
  callback_list_node_t *tail;
};

/**
 * the context handle
 * @todo what is a context?
 */
struct hpx_context {
  hpx_context_id_t            cid;              /*!<  */
  hpx_kthread_t            **kths;              /*!< kthread descriptors */
  void                **kths_args;              /*!< kthread arguments */
  uint32_t             kths_count;              /*!<  */
  uint32_t               kths_idx;              /*!<  */

  hpx_kthread_mutex_t         mtx;              /*!<  */

  hpx_config_t                cfg;              /*!<  */
  hpx_mconfig_t              mcfg;              /*!<  */
  uint64_t                 mflags;              /*!<  */

  hpx_queue_t            term_ths;              /*!<  */
  hpx_queue_t           term_stks;              /*!<  */
  hpx_queue_t           term_lcos;              /*!<  */

  /* service threads */
  struct hpx_thread    **srv_susp;              /*!<  */
  struct hpx_thread    *srv_rebal;              /*!<  */

  hpx_future_t         f_srv_susp;              /*!<  */
  hpx_future_t        f_srv_rebal;              /*!<  */

  callback_list_t kthread_on_init;              /*!< kthread initializers */
  callback_list_t kthread_on_fini;              /*!< kthread finalizers  */
  struct sr_barrier      *barrier;              /*!< kthread barrier */
};

hpx_context_t *hpx_ctx_create(hpx_config_t *context);
void hpx_ctx_destroy(hpx_context_t *);
hpx_context_id_t hpx_ctx_get_id(hpx_context_t *);

#endif /* HPX_CONTEXT_H_ */

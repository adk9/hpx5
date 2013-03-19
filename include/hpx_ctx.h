
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
#ifndef LIBHPX_CONTEXT_H_
#define LIBHPX_CONTEXT_H_

#include <stdint.h>
#include "hpx_queue.h"
#include "hpx_kthread.h"

#ifdef __x86_64__
  #include "arch/x86_64/hpx_mconfig.h"
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

/* the global next context ID */
static hpx_context_id_t __ctx_next_id;

/* the context handle */ 
typedef struct {
  hpx_context_id_t cid;

  hpx_queue_t q_pend;
  hpx_queue_t q_exe;
  hpx_queue_t q_block;
  hpx_queue_t q_susp;
  hpx_queue_t q_term;

  hpx_kthread_t ** kths;
  long kths_count;

  hpx_mconfig_t mcfg;
} hpx_context_t;


/*
 --------------------------------------------------------------------
  Context Functions
 --------------------------------------------------------------------
*/

hpx_context_t * hpx_ctx_create(void);
void hpx_ctx_destroy(hpx_context_t *);
hpx_context_id_t hpx_ctx_get_id(hpx_context_t *);

#endif



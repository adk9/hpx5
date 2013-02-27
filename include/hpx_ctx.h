
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
#include "hpx_heap.h"


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
  hpx_heap_t q_pend;
  hpx_heap_t q_exe;
  hpx_heap_t q_block;
  hpx_heap_t q_susp;
  hpx_heap_t q_term;
} hpx_context_t;


/*
 --------------------------------------------------------------------
  hpx_ctx_create

  Creates and initializes a new scheduling context.
 --------------------------------------------------------------------
*/

hpx_context_t * hpx_ctx_create(void);


/*
 --------------------------------------------------------------------
  hpx_ctx_destroy

  Destroys a previously created scheduling context.
 --------------------------------------------------------------------
*/

void hpx_ctx_destroy(hpx_context_t *);


/*
 --------------------------------------------------------------------
  hpx_ctx_get_id

  Gets the ID of the supplied context.
 --------------------------------------------------------------------
*/

hpx_context_id_t hpx_ctx_get_id(hpx_context_t *);

#endif



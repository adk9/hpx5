
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Function Definitions
  hpx_thread.h

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
#ifndef LIBHPX_THREAD_H_
#define LIBHPX_THREAD_H_

#include <stdint.h>
#include "hpx_mem.h"
#include "hpx_ctx.h"


typedef uint64_t hpx_node_id_t;
typedef uint64_t hpx_thread_id_t;
typedef uint8_t  hpx_thread_state_t;


/*
 --------------------------------------------------------------------
  Thread Data

  nid                          Node ID
  tid                          Thread ID
  state                        Bits 0-3:  Queuing State
                                          0 = Suspended
                                          1 = Pending
                                          2 = Executing
                                          3 = Blocked
                                          4 = Terminated
                               Bits 4-7:  Reserved
 --------------------------------------------------------------------
*/

typedef struct {
  hpx_node_id_t      nid;
  hpx_thread_id_t    tid;
  hpx_thread_state_t state;
} hpx_thread_t;


/*
 --------------------------------------------------------------------
  hpx_thread_create

  Creates and initializes a thread.  
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t *);


/*
 --------------------------------------------------------------------
  hpx_thread_destroy

  Destroys a previously created thread.
 --------------------------------------------------------------------
*/

void hpx_thread_destroy(hpx_thread_t *);

#endif



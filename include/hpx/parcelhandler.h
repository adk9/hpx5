/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Function Definitions
  hpx_parchandler.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of 
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_PARCHANDLER_H_
#define LIBHPX_PARCHANDLER_H_

#include "hpx/mem.h"
#include "hpx/thread.h"


/*
 --------------------------------------------------------------------
  Parcel Handler Data

  ctx                          Pointer to the thread context
  thread                       Pointer to the parcel handler's thread
 --------------------------------------------------------------------
*/

typedef struct hpx_parchandler_t {
  hpx_context_t *ctx;
  hpx_thread_t *thread;  
} hpx_parchandler_t;

/*
 --------------------------------------------------------------------
  Parcel Handler Functions
 --------------------------------------------------------------------
*/

hpx_parchandler_t *hpx_parchandler_create();
void hpx_parchandler_destroy(hpx_parchandler_t *);

/*
 --------------------------------------------------------------------
  Private Functions
 --------------------------------------------------------------------
*/

/* TODO: should _hpx_parchandler_main be here? */

#endif /* LIBHPX_PARCELHANDLER_H_ */


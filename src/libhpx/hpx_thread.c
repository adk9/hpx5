
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Functions
  hpx_thread.c

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


#include "hpx_thread.h"
#include "hpx_error.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_thread_create
 --------------------------------------------------------------------
*/

hpx_thread_t * hpx_thread_create(hpx_context_t * ctx) {
  hpx_thread_t * th = NULL;

  th = (hpx_thread_t *) hpx_alloc(sizeof(hpx_thread_t));
  if (th != NULL) {
    memset(th, 0, sizeof(hpx_thread_t));
    
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }
}


/*
 --------------------------------------------------------------------
  hpx_thread_destroy
 --------------------------------------------------------------------
*/

void hpx_thread_destroy(hpx_thread_t * th) {
  hpx_free(th);
}

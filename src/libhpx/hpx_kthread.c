
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  "Kernel" Thread Functions
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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "hpx_kthread.h"
#include "hpx_error.h"
#include "hpx_mem.h"


/*
 --------------------------------------------------------------------
  hpx_kthread_seed_default

  A default seed function for new kernel threads.
 --------------------------------------------------------------------
*/

void * hpx_kthread_seed_default(void * ptr) {
  for (;;) { }
}


/*
 --------------------------------------------------------------------
  hpx_kthread_create

  Creates a kernel thread and executes the provided seed function.
 --------------------------------------------------------------------
*/

hpx_kthread_t * hpx_kthread_create(hpx_kthread_seed_t seed) {
  hpx_kthread_t * kth = NULL;
  int err;

  /* allocate and init the handle */
  kth = (hpx_kthread_t *) hpx_alloc(sizeof(hpx_kthread_t));
  if (kth != NULL) {
    memset(kth, 0, sizeof(hpx_kthread_t));
    
    /* create the thread */
    err = pthread_create(&kth->th, NULL, seed, NULL);
    if (err != 0) {
      hpx_free(kth);
      kth = NULL;

      switch (err) {
        case EAGAIN:
          __hpx_errno = HPX_ERROR_KTH_MAX;
	  break;
        case EINVAL:
          __hpx_errno = HPX_ERROR_KTH_ATTR;
	  break;
        default:
	  __hpx_errno = HPX_ERROR_KTH_INIT;
	  break;
      }
    } else {

    }
  } else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }

  return kth;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_get_affinity

  Gets the logical CPU affinity for a given kernel thread.
 --------------------------------------------------------------------
*/

uint16_t hpx_kthread_get_affinity(hpx_kthread_t * kth) {
  return 0;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_set_affinity

  Sets the logical CPU affinity for a given kernel thread.
 --------------------------------------------------------------------
*/

void hpx_kthread_set_affinity(hpx_kthread_t * kth, uint16_t aff) {

}


/*
 --------------------------------------------------------------------
  hpx_kthread_destroy

  Terminates and destroys a previously created kernel thread.
 --------------------------------------------------------------------
*/

void hpx_kthread_destroy(hpx_kthread_t * kth) {
  pthread_detach(kth->th);
  pthread_cancel(kth->th);
  hpx_free(kth);
  kth = NULL;
}


/*
 --------------------------------------------------------------------
  hpx_kthread_get_cores

  Returns the number of logical compute cores on this machine.
 --------------------------------------------------------------------
*/

long hpx_kthread_get_cores(void) {
  long cores = 0;

#ifdef __linux__                                       /* Linux */
  cores = sysconf(_SC_NPROCESSORS_ONLN);
#elif __APPLE__ && __MACH__                           /* Mac OS X */
  cores = sysconf(_SC_NPROCESSORS_ONLN);
#endif

  return cores;
}

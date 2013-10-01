/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  The HPX 5 Runtime (Locality operations)
  locality.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#include <stdlib.h>

#include "hpx/action.h"
#include "hpx/agas.h"
#include "hpx/error.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

static hpx_locality_t *__hpx_my_locality = NULL;

hpx_locality_t *hpx_locality_create(void) {
  hpx_locality_t *loc = NULL;

  loc = (hpx_locality_t*)hpx_alloc(sizeof(hpx_locality_t));
  if (loc != NULL) {
    memset(loc, 0, sizeof(hpx_locality_t));
    
  }
  else {
    __hpx_errno = HPX_ERROR_NOMEM;
  }
  return loc;
}

void hpx_locality_destroy(hpx_locality_t* loc) {
  hpx_free(loc);
}

hpx_locality_t *hpx_get_my_locality(void) {
  if (__hpx_my_locality == NULL) {
    __hpx_my_locality = hpx_locality_create();
    /* TODO: replace with real runtime configured rank setting */
#if HAVE_MPI
    __hpx_my_locality->rank = _get_rank_mpi();
#endif
#if HAVE_PHOTON
    __hpx_my_locality->rank = _get_rank_photon();
#endif
  }
  return __hpx_my_locality;
}

hpx_locality_t *hpx_get_locality(int rank) {
  hpx_locality_t *loc = hpx_locality_create();
  /* TODO: replace with real runtime configured ranks */
#if HAVE_MPI
  loc->rank = (uint32_t)rank;
#endif
#if HAVE_PHOTON
  loc->rank = (uint32_t)rank;
#endif
}

uint32 hpx_get_num_localities(void) {
  // ask the network layer for the number of localities
  /* TODO: replace with real runtime configured ranks */
	uint32 ranks;
#if HAVE_MPI
	ranks = _get_size_mpi();
#endif
#if HAVE_PHOTON
	ranks =  _get_size_photon();
#endif
	return ranks;
}

uint32 hpx_get_rank(void) {
  return hpx_get_my_locality()->rank;
}

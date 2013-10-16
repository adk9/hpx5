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

#include "bootstrap/bootstrap.h"
#include "network/network.h"
#include "hpx/action.h"
#include "hpx/agas.h"
#include "hpx/error.h"
#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

static hpx_locality_t *my_locality = NULL;

hpx_locality_t *hpx_locality_create(void) {
  hpx_locality_t *loc = NULL;

  loc = (hpx_locality_t*)hpx_alloc(sizeof(hpx_locality_t));
  if (loc != NULL)
    memset(loc, 0, sizeof(hpx_locality_t));
  else
    __hpx_errno = HPX_ERROR_NOMEM;

  return loc;
}

void hpx_locality_destroy(hpx_locality_t* loc) {
  hpx_free(loc);
}

hpx_locality_t *hpx_get_my_locality(void) {
  if (my_locality == NULL) {
    my_locality = hpx_locality_create();
    /* TODO: replace with real runtime configured rank setting */
    __hpx_network_ops->phys_addr(my_locality);
  }
  return my_locality;
}

hpx_locality_t *hpx_find_locality(int rank) {
  hpx_locality_t *l;

  l = hpx_locality_create();
#if 0
  /* BDM I removed this code because it was not working. It looks
     sound, but for some reason, bootstrap_mpi_get_map is getting back
     bad values from MPI_Allgather. I'm not sure why that is
     happening. But also, this value really needs to get cached for
     performance reasons. I suggest we just have an array of all
     localities created at startup and then instead of creating new
     localities, we can just give back a pointer to existing ones. */
  int ret;
  hpx_locality_t locs, *l, *m;
  ret = bootmgr->get_map(&locs);
  if (ret != 0)
    return NULL;

  m = &locs[rank];
  if (!m)
    memcpy(l, m, sizeof(*l));

  free(locs);
#endif
  l->rank = rank; // TODO: remove when we put the above code back in
  return l;          
}

uint32 hpx_get_num_localities(void) {
  // ask the network layer for the number of localities
  /* TODO: replace with real runtime configured ranks */
  return (uint32)bootmgr->size();
}

uint32 hpx_get_rank(void) {
  return hpx_get_my_locality()->rank;
}

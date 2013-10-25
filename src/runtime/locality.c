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

hpx_locality_t* hpx_locality_from_rank(int rank) {
  hpx_locality_t *l;
  l = hpx_locality_create();
  if (!l) return NULL;
  l->rank = rank;
  return l;
}

hpx_locality_t *hpx_find_locality(int rank) {
  int ret;
  hpx_locality_t *locs, *l, *m;

  l = hpx_locality_create();
  if (!l) return NULL;
  ret = bootmgr->get_map(&locs);
  if (ret != 0)
    return NULL;

  m = &locs[rank];
  if (!m)
    memcpy(l, m, sizeof(*l));

  free(locs);
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

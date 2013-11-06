/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  mpirun bootstrap component
  mpirun.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>
#include <limits.h>
#include <stdlib.h>

#if HAVE_PMI
#include <pmi.h>
#endif

#include "bootstrap/bootstrap.h"
#include "network.h"
#include "hpx/action.h"
#include "hpx/init.h"
#include "hpx/parcel.h"
#include "hpx/runtime.h"

#include "bootstrap_pmi.h"

static int rank;
static int size;

/* PMI bootstrap operations */
bootstrap_ops_t pmi_boot_ops = {
  .init     = bootstrap_pmi_init,
  .get_rank = bootstrap_pmi_get_rank,
  .get_addr = bootstrap_pmi_get_addr,
  .get_map  = bootstrap_pmi_get_map,
  .size     = bootstrap_pmi_size,
  .finalize = bootstrap_pmi_finalize,
};

int bootstrap_pmi_init(void) {
  int ret;
  int spawned;
  PMI_BOOL init;

  PMI_Initialized(&init);
  if (init != PMI_TRUE)
    if (PMI_Init(&spawned) != PMI_SUCCESS)
      goto err;
  if (spawned) {} else {}

  if (PMI_Get_size(&size) != PMI_SUCCESS)
    goto err;

  if (PMI_Get_rank(&rank) != PMI_SUCCESS)
    goto err;

  return 0;

err:
  return HPX_ERROR;
}

int bootstrap_pmi_get_rank(void) {
  return rank;
}

int bootstrap_pmi_get_addr(hpx_locality_t *l) {
  return __hpx_network_ops->phys_addr(l);
}

int bootstrap_pmi_size(void) {
  return size;
}

int bootstrap_pmi_get_map(hpx_locality_t **map) {
  int ret;
  hpx_locality_t *loc;

  *map = NULL;
  loc = hpx_get_my_locality();
  if (!loc) return HPX_ERROR;

  *map = hpx_alloc(size * sizeof(hpx_locality_t));
  if (*map == NULL) return HPX_ERROR_NOMEM;

#if HAVE_PMI_CRAY_EXT
  /* ADK: NB. the data gathered is not necessarily in process
     rank order. it could also be different across all ranks. */
  ret = PMI_Allgather(loc, *map, sizeof(*loc));
  if (ret != PMI_SUCCESS) {
    free(*map);
    *map = NULL;
    return HPX_ERROR;
  }
#endif

  return 0;
}

int bootstrap_pmi_finalize(void) {
  return PMI_Finalize();
}

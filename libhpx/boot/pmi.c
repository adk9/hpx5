// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <pmi.h>
#include "manager.h"

static void _delete(manager_t *pmi) {
  PMI_Finalize();
  free(pmi);
}

manager_t *
manager_new_pmi(void) {
  PMI_BOOL init;
  PMI_Initialized(&init);
  if (init != PMI_TRUE) {
    int spawned;
    if (PMI_Init(&spawned) != PMI_SUCCESS)
      return NULL;
  }

  int rank;
  if (PMI_Get_rank(&rank) != PMI_SUCCESS)
    return NULL;

  int n_ranks;
  if (PMI_Get_size(&size) != PMI_SUCCESS)
    return NULL;

  manager_t *pmi = malloc(sizeof(*pmi));
  pmi->delete = _delete;
  pmi->rank = rank;
  pmi->n_ranks = n_ranks;
  return pmi;
}

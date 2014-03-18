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
#include "config.h"
#endif

#include <stdlib.h>
#include "manager.h"

static void _delete(manager_t *smp) {
  free(smp);
}

manager_t *
manager_new_smp(void) {
  manager_t *smp = malloc(sizeof(*smp));
  smp->delete    = _delete;
  smp->rank      = 0;
  smp->n_ranks   = 1;
  return smp;
}

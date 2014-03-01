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
#ifndef LIBHPX_MANAGER_H
#define LIBHPX_MANAGER_H

#include "attributes.h"

typedef struct manager {
  void (*delete)(struct manager *);
  int rank;
  int n_ranks;
} manager_t;

HPX_INTERNAL manager_t *manager_new_mpirun(void);
HPX_INTERNAL manager_t *manager_new_pmi(void);
HPX_INTERNAL manager_t *manager_new_smp(void);

#endif // LIBHPX_MANAGER_H

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

/// ----------------------------------------------------------------------------
/// @file libhpx/locality/manager.c
/// @brief Handles manager initialization.
/// ----------------------------------------------------------------------------
#include "locality.h"
#include "manager.h"

manager_t *
manager_new(void) {
  manager_t *manager = NULL;

#ifdef HAVE_PMI
  manager = manager_new_pmi();
  if (manager) {
    locality_logf("initialized PMI process manager.\n");
    return manager;
  }
#endif

#ifdef HAVE_MPI
  manager = manager_new_mpirun();
  if (manager) {
    locality_logf("initialized MPI-run process manager.\n");
    return manager;
  }
#endif

  manager = manager_new_smp();
  if (manager) {
    locality_logf("initialized the SMP process manager.\n");
    return manager;
  }

  locality_printe("failed to initialize a process manager.\n");
  return NULL;
}

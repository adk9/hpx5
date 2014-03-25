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
/// @file libhpx/locality/boot.c
/// @brief Handles boot initialization.
/// ----------------------------------------------------------------------------
#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "managers.h"

boot_t *boot_new(void) {
  boot_t *boot = NULL;

#ifdef HAVE_PMI
  boot = boot_new_pmi();
  if (boot) {
    dbg_log("initialized PMI process boot manager.\n");
    return boot;
  }
#endif

#ifdef HAVE_MPI
  boot = boot_new_mpi();
  if (boot) {
    dbg_log("initialized MPI-run process boot manager.\n");
    return boot;
  }
#endif

  boot = boot_new_smp();
  if (boot) {
    dbg_log("initialized the SMP process boot manager.\n");
    return boot;
  }

  dbg_error("failed to initialize a process boot manager.\n");
  return NULL;
}

void boot_delete(boot_t *boot) {
  boot->delete(boot);
}

int boot_rank(const boot_t *boot) {
  return (boot ? boot->rank(boot) : -1);
}

int boot_n_ranks(const boot_t *boot) {
  return (boot ? boot->n_ranks(boot) : -1);
}

int boot_barrier(const boot_t *boot) {
  return boot->barrier();
}

int boot_allgather(const boot_t *boot, const void *in, void *out, int n) {
  return boot->allgather(boot, in, out, n);
}

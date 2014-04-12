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

boot_class_t *boot_new(void) {
  boot_class_t *boot = NULL;

#ifdef HAVE_PMI
  boot = boot_new_pmi();
  if (boot) {
    dbg_log("initialized PMI process boot manager.\n");
    return boot;
  }
#endif

#if defined(HAVE_MPI) || defined(HAVE_PHOTON)
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


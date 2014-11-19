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

#ifdef ENABLE_TAU
#define TAU_DEFAULT 1
#include <TAU.h>
#endif

static boot_class_t *_default(void) {
#ifdef HAVE_PMI
  return boot_new_pmi();
#endif

#ifdef HAVE_MPI
  return boot_new_mpi();
#endif

  return boot_new_smp();
}

boot_class_t *boot_new(hpx_boot_t type) {
#ifdef ENABLE_TAU
          TAU_START("boot_new");
#endif
  boot_class_t *boot = NULL;

  switch (type) {
   case (HPX_BOOT_PMI):
#ifdef HAVE_PMI
    boot = boot_new_pmi();
    if (boot)
      dbg_log_boot("initialized PMI bootstrapper.\n");
#else
    dbg_error("PMI bootstrap not supported in current configuration.\n");
#endif
    break;

   case (HPX_BOOT_MPI):
#ifdef HAVE_MPI
    boot = boot_new_mpi();
    if (boot)
      dbg_log_boot("initialized mpirun bootstrapper.\n");
#else
    dbg_error("MPI bootstrap not supported in current configuration.\n");
#endif
    break;

   case (HPX_BOOT_SMP):
    boot = boot_new_smp();
    if (boot)
      dbg_log_boot("initialized the SMP bootstrapper.\n");
    break;

   case HPX_BOOT_DEFAULT:
   default:
    boot = _default();
    break;
  }

  if (!boot) {
    boot = _default();
  }

  if (!boot) {
    dbg_error("failed to initialize the bootstrapper.\n");
  }
  else {
    dbg_log("bootstrapped using %s.\n", boot->id());
  }
#ifdef ENABLE_TAU
          TAU_STOP("boot_new");
#endif
  return boot;
}


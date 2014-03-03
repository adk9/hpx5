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
#include <stddef.h>
#include "attributes.h"

/// ----------------------------------------------------------------------------
/// @file libhpx/network/transport.c
/// @brief "placeholder" weak implementations to prevent linker complaints
/// ----------------------------------------------------------------------------

HPX_INTERNAL void *transport_new_photon(void) HPX_WEAK;
HPX_INTERNAL void *transport_new_mpi(void) HPX_WEAK;
HPX_INTERNAL void *manager_new_smp(void) HPX_WEAK;

void *
transport_new_photon(void) {
  return NULL;
}

void *
transport_new_mpi(void) {
  return NULL;
}

void *
manager_new_smp(void) {
  return NULL;
}

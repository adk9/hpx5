// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#ifndef LIBHPX_PROCESS_H
#define LIBHPX_PROCESS_H

#include <hpx/hpx.h>

/// SPMD epoch management.
/// @{
///
/// ParalleX processes are envisioned as diffusive entitieis, where execution
/// spreads through the system from a single thread, however many current
/// applications benefit from a SPMD model where execution starts as a set of
/// symmetric top-level actions and terminates when these actions collectively
/// terminate.
///
/// Termination detection in such a setting can be performed without credit
/// recovery, but still requires a notion of quiescence. HPX-5 SPMD processes
/// are only available from top-level hpx_run_spmd() epochs, and as such we can
/// use an extremely simple form of centralized reference counting for
/// quiescence.
///
/// @todo: SPMD behavior should be part of the process namespace, and available
///        for any process launch.
///
void spmd_init(void);
void spmd_fini(void);
extern HPX_ACTION_DECL(spmd_epoch_terminate);
/// @}

/// Recover any credit associated with a parcel.
int process_recover_credit(hpx_parcel_t *p)
  HPX_NON_NULL(1);

#endif // LIBHPX_PROCESS_H

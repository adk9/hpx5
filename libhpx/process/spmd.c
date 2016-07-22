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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>
#include <pthread.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/process.h>

/// Centralized SPMD termination detection.
///
/// This is about as simple a termination algorithm as possible. There are more
/// much more scalable ways to do this using collectives. But we don't really
/// have the right functionality in either the bootstrap or high-speed network
/// to support such behavior. In particular, a collective implementation needs
/// to be non-blocking and integrated into the scheduler such that every
/// locality keeps processing while checking for the global termination.

static uint32_t _count = 0;
static uint32_t _code = 0;
static pthread_mutex_t _lock = PTHREAD_MUTEX_INITIALIZER;

void
spmd_init(void)
{
  pthread_mutex_lock(&_lock);
  _count = 0;
  _code = HPX_SUCCESS;
  pthread_mutex_unlock(&_lock);
}

void
spmd_fini(void)
{
  pthread_mutex_lock(&_lock);
  dbg_assert(_count == 0);
  pthread_mutex_unlock(&_lock);
}

static int
_spmd_terminate(uint32_t *code)
{
  int count = 0;
  pthread_mutex_lock(&_lock);
  _code = (_code) ? _code : *code;
  *code = _code;
  count = _count += 1;
  dbg_assert(count <= here->ranks);
  if (count == here->ranks) {
    _count = 0;
    _code = HPX_SUCCESS;
  }
  pthread_mutex_unlock(&_lock);
  return (count == here->ranks);
}

static int
_spmd_epoch_terminate_handler(uint64_t code)
{
  dbg_assert(here->rank == 0);
  dbg_assert(code != UINT64_MAX);

  uint32_t int_code = (uint32_t)code;
  log_net("received shutdown spmd (code %" PRIu32 ")\n", int_code);

  if (_spmd_terminate(&int_code)) {
    hpx_exit(int_code, 0, NULL);
  }

  return HPX_SUCCESS;
}
LIBHPX_ACTION(HPX_INTERRUPT, 0, spmd_epoch_terminate,
              _spmd_epoch_terminate_handler, HPX_UINT64);

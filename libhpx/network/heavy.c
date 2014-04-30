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

#include <pthread.h>

#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "servers.h"

/// ----------------------------------------------------------------------------
/// The heavy network thread just loops in network_progress(), and tests for
/// cancellation and yields each time through the loop.
///
/// This structure isn't great, but is not terrible for a funneled transport
/// implementation.
/// ----------------------------------------------------------------------------
void* heavy_network(void *args) {
  while (true) {
    transport_progress(here->transport, false);
    pthread_testcancel();
#if defined(__APPLE__) || defined(__MACH__)
    pthread_yield_np();
#else
    pthread_yield();
#endif
  }

  return NULL;
}

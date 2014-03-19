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

#include "libhpx/network.h"
#include "heavy.h"

/// ----------------------------------------------------------------------------
/// The heavy network thread just loops in network_progress(), and tests for
/// cancellation and yields each time through the loop.
///
/// This structure isn't great, but is not terrible for a funneled transport
/// implementation.
/// ----------------------------------------------------------------------------
void* heavy_network(void *args) {
  network_t *network = args;

  while (1) {
    pthread_testcancel();
    network_progress(network);
    pthread_yield();
  }
}

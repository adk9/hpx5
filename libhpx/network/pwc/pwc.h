// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include "libhpx/padding.h"
#include "libhpx/parcel.h"

/// Forward declarations.
/// @{
extern "C" {
struct boot;
struct config;
struct gas;
}
/// @}

namespace libhpx {
namespace network {
namespace pwc {

class Command;
struct heap_segment_t;
struct pwc_xport_t;
struct parcel_emulator_t;
struct send_buffer_t;
struct headp_segment_t;

struct pwc_network_t {
  pwc_xport_t            *xport;
  parcel_emulator_t    *parcels;
  send_buffer_t   *send_buffers;
  heap_segment_t *heap_segments;
  PAD_TO_CACHELINE(4 * sizeof(void*));
  volatile int probe_lock;
  PAD_TO_CACHELINE(sizeof(int));
  volatile int progress_lock;
  PAD_TO_CACHELINE(sizeof(int));
};

/// Allocate and initialize a PWC network instance.
pwc_network_t *network_pwc_funneled_new(const struct config *cfg,
                                        struct boot *boot, struct gas *gas,
                                        pwc_xport_t *xport)
  HPX_MALLOC;

void pwc_deallocate(void *network);
int pwc_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync);
hpx_parcel_t * pwc_probe(void *network, int rank);
void pwc_progress(void *network, int id);

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif

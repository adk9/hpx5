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
#ifndef LIBHPX_LOCALITY_H
#define LIBHPX_LOCALITY_H

/// ----------------------------------------------------------------------------
/// Exports all of the resources available at an HPX locality.
/// ----------------------------------------------------------------------------

#include "hpx/hpx.h"
#include "libsync/sync.h"

struct boot_class;
struct btt_class;
struct gas_class;
struct network_class;
struct scheduler;
struct transport_class;

typedef struct {
  int                          rank;            // this locality's rank
  int                         ranks;            // the total number of ranks
  void                       *local;            // the local data segment
  struct boot_class           *boot;            // the bootstrap object
  struct btt_class             *btt;            // the block translation table
  struct transport_class *transport;            // the byte transport
  struct network_class     *network;            // the parcel transport
  struct scheduler           *sched;            // the scheduler data

  SYNC_ATOMIC(uint32_t)  local_sbrk;            // the local memory block sbrk
  SYNC_ATOMIC(uint32_t) global_sbrk;            // the global block id sbrk
} locality_t;


/// Actions for use with HPX_THERE()
HPX_INTERNAL extern hpx_action_t locality_shutdown;
HPX_INTERNAL extern hpx_action_t locality_global_sbrk;
HPX_INTERNAL extern hpx_action_t locality_alloc_blocks;

HPX_INTERNAL extern hpx_action_t locality_gas_acquire;
HPX_INTERNAL extern hpx_action_t locality_gas_move;
typedef struct {
  hpx_addr_t addr;
  uint32_t rank;
} locality_gas_forward_args_t;
HPX_INTERNAL extern hpx_action_t locality_gas_forward;

/// ----------------------------------------------------------------------------
/// The global locality is exposed through this "here" pointer.
///
/// The value of the pointer is equivalent to hpx_addr_try_pin(HPX_HERE, &here);
/// ----------------------------------------------------------------------------
HPX_INTERNAL extern locality_t *here;


#endif // LIBHPX_LOCALITY_H

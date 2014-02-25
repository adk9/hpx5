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
/// @file network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include "transport/transport.h"
#include "network.h"
#include "parcel.h"
#include "scheduler.h"
#include "thread.h"
#include "debug.h"

const hpx_addr_t HPX_NULL = { NULL, -1 };

static transport_t *_transport = NULL;
static int _rank = -1;
static int _num_ranks = -1;
static hpx_action_t _network_free = 0;

static int _network_free_action(void *args);

int
network_init(void) {
  _transport = smp_new();
  _rank = 0;
  _num_ranks = 1;
  _network_free = hpx_action_register("_network_free", _network_free_action);
  return HPX_SUCCESS;
}

int
network_init_thread(void) {
  return HPX_SUCCESS;
}

void
network_fini(void) {
  smp_delete(_transport);
}

void
network_fini_thread(void) {
}

// static void *_send_offset(hpx_parcel_t *parcel) {
//   return &parcel->size;
// }

// static int _send_size(hpx_parcel_t *parcel) {
//   int size = parcel->size;
//   size += sizeof(parcel->size);
//   size += sizeof(parcel->action);
//   size += sizeof(parcel->target);
//   size += sizeof(parcel->cont);
//   return size;
// }

void
hpx_parcel_send(hpx_parcel_t *p) {
  if (network_addr_is_local(p->target, NULL))
    scheduler_spawn(p);
  else
    UNIMPLEMENTED();
}

void
hpx_parcel_send_sync(hpx_parcel_t *p) {
}

void
network_berrier(void) {
}

int
hpx_get_my_rank(void) {
  return _rank;
}

int
hpx_get_num_ranks(void) {
  return _num_ranks;
}

bool
network_addr_is_local(hpx_addr_t addr, void **out) {
  if (addr.rank != hpx_get_my_rank())
    return false;

  if (out)
    *out = addr.local;

  return true;
}

hpx_addr_t
network_malloc(int size, int alignment) {
  hpx_addr_t addr = { NULL, _rank };
  int e = posix_memalign(&addr.local, alignment, size);
  if (e) {
    fprintf(stderr, "failed global allocation\n");
    abort();
  }
  return addr;
}

static void _free(hpx_addr_t addr) {
  free(addr.local);
}

int
_network_free_action(void *args) {
  thread_t *thread = thread_current();
  hpx_parcel_t *parcel = thread->parcel;
  _free(parcel->target);
  return HPX_SUCCESS;
}

void
network_free(hpx_addr_t addr) {
  if (addr.rank == _rank)
    _free(addr);
  hpx_call(addr, _network_free, NULL, 0, HPX_NULL);
}

bool
hpx_addr_eq(hpx_addr_t lhs, hpx_addr_t rhs) {
  return (lhs.rank == rhs.rank) && (lhs.local == rhs.local);
}

hpx_addr_t
hpx_addr_from_rank(int rank) {
  hpx_addr_t r = { NULL, rank };
  return r;
}

int
hpx_addr_to_rank(hpx_addr_t addr) {
  return addr.rank;
}

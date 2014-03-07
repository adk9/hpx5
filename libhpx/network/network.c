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
/// @file libhpx/libhpx/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "sync/ms_queue.h"
#include "network.h"
#include "transport.h"
#include "debug.h"

const hpx_addr_t HPX_NULL = { NULL, 0 };
const hpx_addr_t HPX_ANYWHERE = { NULL, -1 };

static hpx_action_t _free = 0;
static transport_t *_transport = NULL;
static ms_queue_t _send;

static int _free_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  void *local = NULL;
  if (hpx_addr_try_pin(target, &local)) {
    free(local);
    hpx_addr_unpin(target);
  }
  return HPX_SUCCESS;
}

static void HPX_CONSTRUCTOR _init_network(void) {
  sync_ms_queue_init(&_send);
  _free = hpx_register_action("_hpx_free_action", _free_action);
}

int
network_startup(const hpx_config_t *config) {
  _transport = transport_new();
  return (_transport) ? HPX_SUCCESS : 1;
}

void
network_shutdown(void) {
  _transport->delete(_transport);
}

void
network_berrier(void) {
}

hpx_addr_t
network_malloc(int size, int alignment) {
  hpx_addr_t addr = { NULL, hpx_get_my_rank() };
  int e = posix_memalign(&addr.local, alignment, size);
  if (e) {
    dbg_error("failed global allocation\n");
    abort();
  }
  return addr;
}


void
network_release(hpx_parcel_t *p) {
  free(p);
}

void
network_send(hpx_parcel_t *p) {
  assert(p);
  sync_ms_queue_enqueue(&_send, p);
}


hpx_addr_t
hpx_global_calloc(size_t n, size_t bytes, size_t block_size, size_t alignment) {
  hpx_addr_t addr = {
    NULL,
    hpx_get_my_rank()
  };

  if (posix_memalign(&addr.local, alignment, n * bytes))
    dbg_error("failed global allocation.\n");
  return addr;
}


void
hpx_global_free(hpx_addr_t addr) {
  hpx_call(addr, _free, NULL, 0, HPX_NULL);
}

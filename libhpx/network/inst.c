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

#include <stdlib.h>
#include <libhpx/debug.h>
#include <libhpx/events.h>
#include <libhpx/libhpx.h>
#include <libhpx/network.h>

#include "inst.h"

typedef struct {
  Network vtable;
  void *impl;
} _inst_network_t;

// Define the transports allowed for the SMP network
static void
_inst_deallocate(void *network)
{
  _inst_network_t *inst = network;
  network_delete(inst->impl);
  free(inst);
}

static int
_inst_progress(void *network, int id)
{
  EVENT_NETWORK_PROGRESS_BEGIN();
  _inst_network_t *inst = network;
  int r = network_progress(inst->impl, id);
  EVENT_NETWORK_PROGRESS_END();
  return r;
}

static int
_inst_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  EVENT_NETWORK_SEND();
  _inst_network_t *inst = network;
  return network_send(inst->impl, p, ssync);
}

static hpx_parcel_t *
_inst_probe(void *network, int nrx)
{
  EVENT_NETWORK_PROBE_BEGIN();
  _inst_network_t *inst = network;
  hpx_parcel_t *p = network_probe(inst->impl, nrx);
  EVENT_NETWORK_PROBE_END();
  return p;
}

static void
_inst_flush(void *network)
{
  _inst_network_t *inst = network;
  network_flush(inst->impl);
}

static void
_inst_register_dma(void *network, const void *addr, size_t n, void *key)
{
  _inst_network_t *inst = network;
  network_register_dma(inst->impl, addr, n, key);
}

static void
_inst_release_dma(void *network, const void *addr, size_t n)
{
  _inst_network_t *inst = network;
  network_release_dma(inst->impl, addr, n);
}

static int
_inst_lco_wait(void *network, hpx_addr_t lco, int reset)
{
  _inst_network_t *inst = network;
  return network_lco_wait(inst->impl, lco, reset);
}

static int
_inst_lco_get(void *network, hpx_addr_t lco, size_t n, void *to, int reset)
{
  _inst_network_t *inst = network;
  return network_lco_get(inst->impl, lco, n, to, reset);
}

void*
network_inst_new(void *impl)
{
  dbg_assert(impl);
  _inst_network_t *inst = malloc(sizeof(*inst));
  dbg_assert(inst);

  inst->vtable.string = ((Network*)impl)->string;
  inst->vtable.type = ((Network*)impl)->type;
  inst->vtable.deallocate = _inst_deallocate;
  inst->vtable.progress = _inst_progress;
  inst->vtable.send = _inst_send;
  inst->vtable.probe = _inst_probe;
  inst->vtable.register_dma = _inst_register_dma;
  inst->vtable.release_dma = _inst_release_dma;
  inst->vtable.lco_get = _inst_lco_get;
  inst->vtable.lco_wait = _inst_lco_wait;
  inst->vtable.flush = _inst_flush;

  inst->impl = impl;

  return &inst->vtable;
}

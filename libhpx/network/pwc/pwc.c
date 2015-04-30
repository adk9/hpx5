// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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
#include <hpx/builtins.h>

#include <libhpx/action.h>
#include <libhpx/boot.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>

#include "parcel_emulation.h"
#include "pwc.h"
#include "send_buffer.h"
#include "xport.h"

#include "../commands.h"

typedef struct heap_segment {
  size_t        n;
  char      *base;
  xport_key_t key;
} heap_segment_t;

static HPX_USED const char *_straction(hpx_action_t id) {
  dbg_assert(here && here->actions);
  return action_table_get_key(here->actions, id);
}

static void _probe_local(pwc_network_t *pwc) {
  int rank = here->rank;

  // Each time through the loop, we deal with local completions.
  command_t command;
  while (pwc->xport->test(&command, NULL)) {
    hpx_addr_t op = command_get_op(command);
    log_net("processing local command: %s\n", _straction(op));
    int e = hpx_xcall(HPX_HERE, op, HPX_NULL, rank, command);
    dbg_assert_str(HPX_SUCCESS == e, "failed to process local command\n");
  }
}

static hpx_parcel_t *_probe(pwc_network_t *pwc, int rank) {
  command_t command;
  while (pwc->xport->probe(&command, NULL, rank)) {
    hpx_addr_t op = command_get_op(command);
    log_net("processing command %s from rank %d\n", _straction(op), rank);
    int e = hpx_xcall(HPX_HERE, op, HPX_NULL, rank, command);
    dbg_assert_str(HPX_SUCCESS == e, "failed to process command\n");
  }
  return NULL;
}

static int _pwc_progress(void *network) {
  pwc_network_t *pwc = network;
  _probe_local(pwc);
  for (int i = 0, e = here->ranks; i < e; ++i) {
    _probe(pwc, i);
  }
  return 0;
}

/// Probe is used in the generic progress loop to retrieve completed parcels.
///
/// The pwc network currently does all of its parcel processing inline during
/// progress(), so this is a no-op.
static hpx_parcel_t *_pwc_probe(void *network, int rank) {
  return NULL;
}

/// Create a network registration.
static int _pwc_register_dma(void *network, const void *base, size_t n,
                             void *key) {
  pwc_network_t *pwc = network;
  dbg_assert(pwc && pwc->xport && pwc->xport->pin);
  int e = pwc->xport->pin(pwc->xport, base, n, key);
  dbg_check(e, "Could not register (%p, %zu) for rmda\n", base, n);
  return e;
}

/// Release a network registration.
static int _pwc_release_dma(void *network, const void* base, size_t n) {
  pwc_network_t *pwc = network;
  dbg_assert(pwc && pwc->xport && pwc->xport->unpin);
  int e = pwc->xport->unpin(pwc->xport, base, n);
  dbg_check(e, "Could not release (%p, %zu) for rdma\n", base, n);
  return e;
}

typedef struct {
  int        rank;
  hpx_parcel_t *p;
  size_t        n;
  xport_key_t key;
} _rendezvous_get_args_t;

static int _rendezvous_launch_handler(int src, command_t cmd) {
  uintptr_t arg = command_get_arg(cmd);
  hpx_parcel_t *p = (void*)arg;
  parcel_set_state(p, PARCEL_SERIALIZED);
  scheduler_spawn(p);
  return HPX_SUCCESS;
}
COMMAND_DEF(HPX_INTERRUPT, _rendezvous_launch, _rendezvous_launch_handler);

static int _rendezvous_get_handler(size_t size, _rendezvous_get_args_t *args) {
  pwc_network_t *pwc = (pwc_network_t*)here->network;
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, args->n - sizeof(*p));
  dbg_assert(p);
  xport_op_t op = {
    .rank = args->rank,
    .n = args->n,
    .dest = p,
    .dest_key = pwc->xport->key_find_ref(pwc->xport, p, args->n),
    .src = args->p,
    .src_key = &args->key,
    .lop = command_pack(_rendezvous_launch, (uintptr_t)p),
    .rop = command_pack(release_parcel, (uintptr_t)args->p)
  };
  int e = pwc->xport->gwc(&op);
  dbg_check(e, "could not issue get during rendezvous parcel\n");
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_INTERRUPT, HPX_MARSHALLED, _rendezvous_get,
                  _rendezvous_get_handler, HPX_SIZE_T, HPX_POINTER);

static int _pwc_rendezvous_send(pwc_network_t *pwc, hpx_parcel_t *p, int rank) {
  size_t n = parcel_size(p);
  _rendezvous_get_args_t args = {
    .rank = here->rank,
    .p = p,
    .n = n
  };
  int e = pwc->xport->key_find(pwc->xport, p, n, &args.key);
  dbg_check(e, "failed to find an rdma for a parcel (%p)\n", (void*)p);
  hpx_addr_t there = HPX_THERE(rank);
  return hpx_call(there, _rendezvous_get, HPX_NULL, &args, sizeof(args));
}

static int _pwc_send(void *network, hpx_parcel_t *p) {
  pwc_network_t *pwc = network;
  int rank = gas_owner_of(here->gas, p->target);
  if (parcel_size(p) > pwc->cfg->pwc_parceleagerlimit) {
    return _pwc_rendezvous_send(network, p, rank);
  }
  else {
    send_buffer_t *buffer = &pwc->send_buffers[rank];
    return send_buffer_send(buffer, HPX_NULL, p);
  }
}

static int _pwc_command(void *network, hpx_addr_t locality,
                        hpx_action_t rop, uint64_t args) {
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(here->gas, locality);

  xport_op_t op = {
    .rank = rank,
    .n = 0,
    .dest = NULL,
    .dest_key = NULL,
    .src = NULL,
    .src_key = NULL,
    .lop = 0,
    .rop = command_pack(rop, args)
  };

  return pwc->xport->command(&op);
}

static int _pwc_pwc(void *network,
                    hpx_addr_t to, const void *lva, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  pwc_network_t *pwc = (void*)network;
  int rank = gas_owner_of(here->gas, to);
  uint64_t offset = gpa_to_offset(to);

  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = pwc->heap_segments[rank].base + offset,
    .dest_key = &pwc->heap_segments[rank].key,
    .src = lva,
    .src_key = pwc->xport->key_find_ref(pwc->xport, lva, n),
    .lop = command_pack(lop, laddr),
    .rop = command_pack(rop, raddr)
  };

  return pwc->xport->pwc(&op);
}

static int _pwc_put(void *network, hpx_addr_t to, const void *from,
                    size_t n, hpx_action_t lop, hpx_addr_t laddr) {
  hpx_action_t rop = HPX_ACTION_NULL;
  hpx_addr_t raddr = HPX_NULL;
  return _pwc_pwc(network, to, from, n, lop, laddr, rop, raddr);
}

static int _pwc_get(void *network, void *lva, hpx_addr_t from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  pwc_network_t *pwc = network;
  int rank = gas_owner_of(here->gas, from);
  uint64_t offset = gpa_to_offset(from);

  xport_op_t op = {
    .rank = rank,
    .n = n,
    .dest = lva,
    .dest_key = pwc->xport->key_find_ref(pwc->xport, lva, n),
    .src = pwc->heap_segments[rank].base + offset,
    .src_key = &pwc->heap_segments[rank].key,
    .lop = command_pack(lop, laddr),
    .rop = 0
  };

  return pwc->xport->gwc(&op);
}

static void _pwc_set_flush(void *network) {
  // pwc networks always flush their rdma
}

static void _pwc_flush(pwc_network_t *pwc) {
  int remaining;
  command_t command;
  do {
    pwc->xport->test(&command, &remaining);
  } while (remaining > 0);
  boot_barrier(here->boot);
}

static void _pwc_delete(void *network) {
  dbg_assert(network);
  pwc_network_t *pwc = network;
  _pwc_flush(pwc);

  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_fini(&pwc->send_buffers[i]);
  }

  heap_segment_t *heap = &pwc->heap_segments[here->rank];
  _pwc_release_dma(pwc, heap->base, heap->n);
  local_free(pwc->heap_segments);
  local_free(pwc->send_buffers);
  parcel_emulator_delete(pwc->parcels);
  pwc_xport_delete(pwc->xport);
  free(pwc);
}

network_t *network_pwc_funneled_new(const config_t *cfg, boot_t *boot,
                                    gas_t *gas) {
  // Validate parameters.
  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate photon for the SMP boot network\n");
    return NULL;
  }

  // Validate configuration.
  if (cfg->pwc_parceleagerlimit > cfg->pwc_parcelbuffersize) {
    dbg_error(" --hpx-pwc-parceleagerlimit (%zu) must be less than "
              "--hpx-pwc-parcelbuffersize (%zu)\n",
              cfg->pwc_parceleagerlimit, cfg->pwc_parcelbuffersize);
  }


  // Allocate the network object.
  pwc_network_t *pwc = malloc(sizeof(*pwc));
  dbg_assert_str(pwc, "could not allocate put-with-completion network\n");

  // Initialize the network's virtual function table.
  pwc->vtable.type = HPX_NETWORK_PWC;
  pwc->vtable.delete = _pwc_delete;
  pwc->vtable.progress = _pwc_progress;
  pwc->vtable.send = _pwc_send;
  pwc->vtable.command = _pwc_command;
  pwc->vtable.pwc = _pwc_pwc;
  pwc->vtable.put = _pwc_put;
  pwc->vtable.get = _pwc_get;
  pwc->vtable.probe = _pwc_probe;
  pwc->vtable.set_flush = _pwc_set_flush;
  pwc->vtable.register_dma = _pwc_register_dma;
  pwc->vtable.release_dma = _pwc_release_dma;

  // Initialize transports.
  pwc->cfg = cfg;
  pwc->xport = pwc_xport_new(cfg, boot, gas);
  pwc->parcels = parcel_emulator_new_reload(cfg, boot, pwc->xport);
  pwc->send_buffers = local_calloc(here->ranks, sizeof(send_buffer_t));
  pwc->heap_segments = local_calloc(here->ranks, sizeof(heap_segment_t));

  // Register the heap segment.
  heap_segment_t heap = {
    .n = gas_local_size(here->gas),
    .base = gas_local_base(here->gas)
  };
  _pwc_register_dma(pwc, heap.base, heap.n, &heap.key);

  // Exchange all the heap keys
  boot_allgather(boot, &heap, pwc->heap_segments, sizeof(heap));

  // Make sure the exchange went well.
  if (DEBUG) {
    heap_segment_t *segment = &pwc->heap_segments[here->rank];
    dbg_assert(heap.n == segment->n);
    dbg_assert(heap.base == segment->base);
    dbg_assert(!strncmp(heap.key, segment->key, XPORT_KEY_SIZE));
  }

  // Initialize the send buffers.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_t *send = &pwc->send_buffers[i];
    int rc = send_buffer_init(send, i, pwc->parcels, pwc->xport, 8);
    if (LIBHPX_OK != rc) {
      dbg_error("failed to initialize send buffer %d of %u\n", i, e);
    }
  }

  return &pwc->vtable;
}

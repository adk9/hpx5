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

#include "pwc.h"
#include "parcel_emulation.h"
#include "send_buffer.h"
#include "xport.h"
#include <libhpx/action.h>
#include <libhpx/boot.h>
#include "libhpx/collective.h"
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/parcel.h>
#include <hpx/builtins.h>
#include <stdlib.h>
#include <string.h>

namespace {
using namespace libhpx::network::pwc;
}

namespace libhpx {
namespace network {
namespace pwc {
struct heap_segment_t {
  size_t        n;
  char      *base;
  xport_key_t key;
};
} // namespace pwc
} // namespace network
} // namespace libhpx

static void
_probe_local(pwc_network_t *pwc, int id)
{
  int rank = here->rank;

  // Each time through the loop, we deal with local completions.
  Command command;
  int src;
  while (pwc->xport->test(&command, nullptr, XPORT_ANY_SOURCE, &src)) {
    command(rank);
  }
}

static hpx_parcel_t *
_probe(pwc_network_t *pwc, int rank)
{
  Command command;
  int src;
  while (pwc->xport->probe(&command, nullptr, rank, &src)) {
    command(src);
  }
  return nullptr;
}

void
libhpx::network::pwc::pwc_progress(void *network, int id)
{
  pwc_network_t *pwc = (pwc_network_t *)network;
  if (sync_swap(&pwc->progress_lock, 0, SYNC_ACQUIRE)) {
    _probe_local(pwc, id);
    sync_store(&pwc->progress_lock, 1, SYNC_RELEASE);
  }
}

hpx_parcel_t *
libhpx::network::pwc::pwc_probe(void *network, int rank)
{
  pwc_network_t *pwc = (pwc_network_t *)network;
  if (sync_swap(&pwc->probe_lock, 0, SYNC_ACQUIRE)) {
    _probe(pwc, XPORT_ANY_SOURCE);
    sync_store(&pwc->probe_lock, 1, SYNC_RELEASE);
  }
  return nullptr;
}

/// Create a network registration.
void
libhpx::network::pwc::pwc_register_dma(void *network, const void *base, size_t n, void *key)
{
  pwc_network_t *pwc = (pwc_network_t *)network;
  dbg_assert(pwc && pwc->xport && pwc->xport->pin);
  pwc->xport->pin(base, n, key);
}

/// Release a network registration.
void
libhpx::network::pwc::pwc_release_dma(void *network, const void* base, size_t n)
{
  pwc_network_t *pwc = (pwc_network_t *)network;
  dbg_assert(pwc && pwc->xport && pwc->xport->unpin);
  pwc->xport->unpin(base, n);
}

int
libhpx::network::pwc::pwc_coll_init(void *network, void **c)
{
  return LIBHPX_OK;
}

int
libhpx::network::pwc::pwc_coll_sync(void *network, void *in, size_t in_size, void *out, void *ctx)
{
  coll_t *c = (coll_t*)ctx;
  void *sendbuf = in;
  int count = in_size;
  char *comm = c->data + c->group_bytes;
  pwc_network_t *pwc = (pwc_network_t *)network;

  // flushing network is necessary (sufficient ?) to execute any packets
  // destined for collective operation
  pwc_flush(network);

  if (c->type == ALL_REDUCE) {
    pwc->xport->allreduce(sendbuf, out, count, NULL, &c->op, comm);
  }
  return LIBHPX_OK;
}

int
libhpx::network::pwc::pwc_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync)
{
  // This is a blatant hack to keep track of the ssync parcel using p's next
  // pointer. It will allow us to both delete p and run ssync once the
  // underlying network operation is serviced. It works in conjunction with the
  // handle_delete_parcel command.
  dbg_assert(p->next == NULL);
  p->next = ssync;

  if (parcel_size(p) >= here->config->pwc_parceleagerlimit) {
    return pwc_rendezvous_send(network, p);
  }

  pwc_network_t *pwc = (pwc_network_t *)network;
  int rank = gas_owner_of(here->gas, p->target);
  send_buffer_t *buffer = &pwc->send_buffers[rank];
  return send_buffer_send(buffer, p);
}

void
libhpx::network::pwc::pwc_flush(void *pwc)
{
}

void
libhpx::network::pwc::pwc_deallocate(void *network)
{
  dbg_assert(network);
  pwc_network_t *pwc = (pwc_network_t *)network;

  // Cleanup any remaining local work---this can leak memory and stuff, because
  // we aren't actually running the commands that we cleanup.
  int remaining, src;
  Command command;
  do {
    pwc->xport->test(&command, &remaining, XPORT_ANY_SOURCE, &src);
  } while (remaining > 0);

  // Network deletion is effectively a collective, so this enforces that
  // everyone is done with rdma before we go and deregister anything.
  boot_barrier(here->boot);

  // Finalize send buffers.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_fini(&pwc->send_buffers[i]);
  }
  free(pwc->send_buffers);

  if (pwc->heap_segments) {
    heap_segment_t *heap = &pwc->heap_segments[here->rank];
    pwc_release_dma(pwc, heap->base, heap->n);
    free(pwc->heap_segments);
  }

  parcel_emulator_deallocate(pwc->parcels);
  pwc->xport->dealloc(pwc->xport);
  free(pwc);
}

static void
_pwc_register_gas_heap(void *network, boot_t *boot, gas_t *gas)
{
  pwc_network_t *pwc = (pwc_network_t *)network;
  if (gas->type == HPX_GAS_AGAS) {
    pwc->heap_segments = NULL;
    return;
  }

  pwc->heap_segments = (heap_segment_t *)calloc(here->ranks, sizeof(heap_segment_t));

  heap_segment_t heap = {
    .n = gas_local_size(gas),
    .base = (char*)gas_local_base(gas)
  };
  pwc_register_dma(pwc, heap.base, heap.n, &heap.key);

  // Exchange all the heap keys, and make sure it went okay
  boot_allgather(boot, &heap, pwc->heap_segments, sizeof(heap));

  heap_segment_t *segment = &pwc->heap_segments[here->rank];
  dbg_assert(heap.n == segment->n);
  dbg_assert(heap.base == segment->base);
  dbg_assert(!strncmp(heap.key, segment->key, XPORT_KEY_SIZE));

  // avoid unused variable warnings
  (void)segment;
}

pwc_network_t *
libhpx::network::pwc::network_pwc_funneled_new(const config_t *cfg,
                                               boot_t *boot, gas_t *gas)
{
  // Validate parameters.
  if (boot->type == HPX_BOOT_SMP) {
    log_net("will not instantiate PWC for the SMP boot network\n");
    return nullptr;
  }

  // Validate configuration.
  if (popcountl(cfg->pwc_parcelbuffersize) != 1) {
    dbg_error("--hpx-pwc-parcelbuffersize must 2^k (given %zu)\n",
              cfg->pwc_parcelbuffersize);
  }

  if (cfg->pwc_parceleagerlimit > cfg->pwc_parcelbuffersize) {
    dbg_error(" --hpx-pwc-parceleagerlimit (%zu) must be less than "
              "--hpx-pwc-parcelbuffersize (%zu)\n",
              cfg->pwc_parceleagerlimit, cfg->pwc_parcelbuffersize);
  }

  // Allocate the network object and initialize its virtual function table.
  pwc_network_t *pwc = nullptr;
  int e = posix_memalign((void**)&pwc, HPX_CACHELINE_SIZE, sizeof(*pwc));
  dbg_check(e, "failed to allocate the pwc network structure\n");
  dbg_assert(pwc);

  // Initialize locks.
  sync_store(&pwc->probe_lock, 1, SYNC_RELEASE);
  sync_store(&pwc->progress_lock, 1, SYNC_RELEASE);

  // Initialize transports.
  pwc->xport = pwc_xport_new(cfg, boot, gas);
  pwc->parcels = parcel_emulator_new_reload(cfg, boot, pwc->xport);
  pwc->send_buffers = (send_buffer_t*)calloc(here->ranks, sizeof(send_buffer_t));

  // Register the gas heap segment.
  _pwc_register_gas_heap(pwc, boot, gas);

  // Initialize the send buffers.
  for (int i = 0, e = here->ranks; i < e; ++i) {
    send_buffer_t *send = &pwc->send_buffers[i];
    int rc = send_buffer_init(send, i, pwc->parcels, pwc->xport, 8);
    dbg_check(rc, "failed to initialize send buffer %d of %u\n", i, e);
  }

  return pwc;
}

int
libhpx::network::pwc::pwc_get(void *obj, void *lva, hpx_addr_t from, size_t n,
                              const Command& lcmd, const Command& rcmd)
{
  pwc_network_t *pwc = (pwc_network_t *)obj;
  int rank = gpa_to_rank(from);

  xport_op_t op;
  op.rank = rank;
  op.n = n;
  op.dest = lva;
  op.dest_key = pwc->xport->key_find_ref(pwc->xport, lva, n);
  op.src = pwc->heap_segments[rank].base + gpa_to_offset(from);
  op.src_key = &pwc->heap_segments[rank].key;
  op.lop = lcmd;
  op.rop = rcmd;
  return pwc->xport->gwc(&op);
}

int
libhpx::network::pwc::pwc_put(void *obj, hpx_addr_t to, const void *lva,
                              size_t n, const Command& lcmd,
                              const Command& rcmd)
{
  pwc_network_t *pwc = (pwc_network_t *)obj;
  int rank = gpa_to_rank(to);

  xport_op_t op;
  op.rank = rank;
  op.n = n;
  op.dest = pwc->heap_segments[rank].base + gpa_to_offset(to);
  op.dest_key = &pwc->heap_segments[rank].key;
  op.src = lva;
  op.src_key = pwc->xport->key_find_ref(pwc->xport, lva, n);
  op.lop = lcmd;
  op.rop = rcmd;
  return pwc->xport->pwc(&op);
}

int
libhpx::network::pwc::pwc_cmd(void *obj, int rank, const Command& lcmd,
                              const Command& rcmd)
{
  pwc_network_t *pwc = static_cast<pwc_network_t*>(obj);
  return pwc->xport->cmd(rank, lcmd, rcmd);
}

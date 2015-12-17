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
#include <string.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

#include "smp.h"

static const int _zero = 0;

// Define the transports allowed for the SMP network
static void _smp_delete(void *network) {
}

static int _smp_progress(void *network, int id) {
  return 0;
}

static int _smp_send(void *network, hpx_parcel_t *p) {
  hpx_abort();
}

static int _smp_command(void *network, hpx_addr_t rank,
                        hpx_action_t op, uint64_t args) {
  return hpx_xcall(HPX_HERE, op, HPX_NULL, _zero, args);
}

static int _smp_pwc(void *network,
                    hpx_addr_t to, const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  if (n) {
    void *t = (void*)to;
    memcpy(t, from, n);
  }

  if (lop && laddr) {
    dbg_check( hpx_xcall(HPX_HERE, lop, HPX_NULL, _zero, laddr) );
  }

  if (rop && raddr) {
    dbg_check( hpx_xcall(HPX_HERE, rop, HPX_NULL, _zero, raddr) );
  }

  return LIBHPX_OK;
}

static int _smp_put(void *network, hpx_addr_t to,
                    const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  return LIBHPX_EUNIMPLEMENTED;
}

static int _smp_get(void *network, void *to, hpx_addr_t from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  return LIBHPX_EUNIMPLEMENTED;
}

static hpx_parcel_t *_smp_probe(void *network, int nrx) {
  return NULL;
}

static void _smp_flush(void *network) {
}

static void _smp_register_dma(void *obj, const void *addr, size_t n, void *key)
{
}

static void _smp_release_dma(void *obj, const void *addr, size_t n) {
}

static int _smp_lco_wait(void *o, hpx_addr_t lco, int reset) {
  return (reset) ? hpx_lco_wait_reset(lco) : hpx_lco_wait(lco);
}

static int _smp_lco_get(void *o, hpx_addr_t lco, size_t n, void *to, int reset) {
  return (reset) ? hpx_lco_get_reset(lco, n, to) : hpx_lco_get(lco, n, to);
}

static int _smp_coll_init(void *network, coll_t **_c){

  return LIBHPX_OK;
}

static int _smp_coll_sync(void *network, hpx_parcel_t *in, void* out, coll_t* c){
  void *sendbuf = in->buffer;
  int count     = in->size;
  memcpy(out, sendbuf, count);
  return LIBHPX_OK;
}

static network_t _smp = {
  .type = HPX_NETWORK_SMP,
  .delete = _smp_delete,
  .progress = _smp_progress,
  .send = _smp_send,
  .command = _smp_command,
  .pwc = _smp_pwc,
  .put = _smp_put,
  .get = _smp_get,
  .probe = _smp_probe,
  .flush = _smp_flush,
  .register_dma = _smp_register_dma,
  .release_dma = _smp_release_dma,
  .lco_get = _smp_lco_get,
  .lco_wait = _smp_lco_wait,
  .coll_init = _smp_coll_init,
  .coll_sync = _smp_coll_sync
};

network_t *network_smp_new(const struct config *cfg, boot_t *boot) {
  if (boot_n_ranks(boot) > 1) {
    dbg_error("SMP network does not support multiple ranks.\n");
    return NULL;
  }

  return &_smp;
}

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
#include <string.h>
#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>

#include "smp.h"

// Define the transports allowed for the SMP network
static void _smp_deallocate(void *network) {
}

static int _smp_progress(void *network, int id) {
  return 0;
}

static int _smp_send(void *network, hpx_parcel_t *p, hpx_parcel_t *ssync) {
  hpx_abort();
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

static int _smp_coll_init(void *network, void **_c) {
  return LIBHPX_OK;
}

static int _smp_coll_sync(void *network, void *in, size_t in_size, void *out,
                          void *c) {
  void *sendbuf = in;
  int count = in_size;
  memcpy(out, sendbuf, count);
  return LIBHPX_OK;
}

static Network _smp = {
  HPX_NETWORK_SMP,
  nullptr,
  _smp_deallocate,
  _smp_progress,
  _smp_send,
  _smp_coll_init,
  _smp_coll_sync,
  _smp_lco_wait,
  _smp_lco_get,
  _smp_probe,
  _smp_flush,
  _smp_register_dma,
  _smp_release_dma
};

void *network_smp_new(const struct config *cfg, boot_t *boot) {
  if (boot_n_ranks(boot) > 1) {
    dbg_error("SMP network does not support multiple ranks.\n");
    return NULL;
  }

  return &_smp;
}

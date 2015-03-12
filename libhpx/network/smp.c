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

#include <libhpx/boot.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include "smp.h"

// Define the transports allowed for the SMP network
static void _smp_delete(void *network) {
}

static int _smp_progress(void *network) {
  return 0;
}

static int _smp_send(void *network, hpx_parcel_t *p) {
  hpx_abort();
}

static int _smp_command(void *network, hpx_addr_t rank,
                        hpx_action_t op, uint64_t args) {
  static const int zero = 0;
  return hpx_xcall(HPX_HERE, op, HPX_NULL, zero, args);
}

static int _smp_pwc(void *network,
                    hpx_addr_t to, const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  return LIBHPX_EUNIMPLEMENTED;
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

static void _smp_set_flush(void *network) {
}

static network_t _smp = {
  .type = HPX_NETWORK_SMP,
  .transports = NULL,
  .delete = _smp_delete,
  .progress = _smp_progress,
  .send = _smp_send,
  .command = _smp_command,
  .pwc = _smp_pwc,
  .put = _smp_put,
  .get = _smp_get,
  .probe = _smp_probe,
  .set_flush = _smp_set_flush
};

network_t *network_smp_new(const struct config *cfg, boot_t *boot) {
  if (boot_n_ranks(boot) > 1) {
    dbg_error("SMP network does not support multiple ranks.\n");
    return NULL;
  }

  local = address_space_new_default(cfg);
  global = address_space_new_default(cfg);
  registered = address_space_new_default(cfg);

  return &_smp;
}

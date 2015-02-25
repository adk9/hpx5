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
# include "config.h"
#endif

#include <stdlib.h>

#include "libhpx/config.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"

static void _smp_delete(network_t *network) {
}

static int _smp_progress(network_t *network) {
  return 0;
}

static int _smp_send(network_t *network, hpx_parcel_t *p) {
  hpx_abort();
}

static int _smp_command(network_t *network, hpx_addr_t rank,
                        hpx_action_t op, uint64_t args) {
  return hpx_xcall(HPX_HERE, op, HPX_NULL, here->rank, args);
}

static int _smp_pwc(network_t *network,
                    hpx_addr_t to, const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr,
                    hpx_action_t rop, hpx_addr_t raddr) {
  return LIBHPX_EUNIMPLEMENTED;
}

static int _smp_put(network_t *network, hpx_addr_t to,
                    const void *from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  return LIBHPX_EUNIMPLEMENTED;
}

static int _smp_get(network_t *network, void *to, hpx_addr_t from, size_t n,
                    hpx_action_t lop, hpx_addr_t laddr) {
  return LIBHPX_EUNIMPLEMENTED;
}

static hpx_parcel_t *_smp_probe(network_t *network, int nrx) {
  return NULL;
}

static void _smp_set_flush(network_t *network) {
}

static network_t _smp = {
  .type = LIBHPX_NETWORK_SMP,
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

network_t *network_smp_new(void) {
  return &_smp;
}

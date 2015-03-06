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
#ifndef LIBHPX_NETWORK_PWC_XPORT_H
#define LIBHPX_NETWORK_PWC_XPORT_H

#include <libhpx/config.h>

struct boot;

typedef struct pwc_xport {
  hpx_transport_t type;

  void (*delete)(void *xport);

  size_t (*sizeof_rdma_key)(void);
  void (*pin)(void *base, size_t n, void *key);
  void (*unpin)(void *base, size_t n);
  void (*clear)(void *key);
  int (*pwc)(int target, void *rva, const void *lva, size_t n, uint64_t lsync,
             uint64_t rsync, void *rkey);
  int (*gwc)(int target, void *lva, const void *rva, size_t n, uint64_t lsync,
             void *rkey);

  int (*test)(uint64_t *op, int *remaining);
  int (*probe)(uint64_t *op, int *remaining, int rank);
} pwc_xport_t;

pwc_xport_t *pwc_xport_new_photon(const config_t *config, struct boot *boot)
  HPX_INTERNAL;

pwc_xport_t *pwc_xport_new(const config_t *config, struct boot *boot)
  HPX_INTERNAL;

static inline void pwc_xport_delete(pwc_xport_t *xport) {
  xport->delete(xport);
}

#endif // LIBHPX_NETWORK_PWC_XPORT_H

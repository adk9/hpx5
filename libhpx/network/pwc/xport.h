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
#include <libhpx/memory.h>

struct boot;
struct gas;

#define XPORT_KEY_SIZE 16

typedef char xport_key_t[XPORT_KEY_SIZE];

typedef struct xport_op {
  int             rank;
  int   UNUSED_PADDING;
  size_t             n;
  void           *dest;
  const void *dest_key;
  const void      *src;
  const void  *src_key;
  uint64_t         lop;
  uint64_t         rop;
} xport_op_t HPX_ALIGNED(HPX_CACHELINE_SIZE);

typedef struct pwc_xport {
  hpx_transport_t type;

  void (*delete)(void *xport);
  const void *(*key_find_ref)(const void *xport, const void *addr, size_t n);
  int (*key_find)(const void *xport, const void *addr, size_t n, void *key);
  int (*key_copy)(void *restrict dest, const void *restrict src);
  int (*key_clear)(void *key);
  int (*command)(const xport_op_t *op);
  int (*pwc)(xport_op_t *op);
  int (*gwc)(xport_op_t *op);
  int (*test)(uint64_t *op, int *remaining);
  int (*probe)(uint64_t *op, int *remaining, int rank);
  memory_register_t  pin;
  memory_release_t unpin;
} pwc_xport_t;

pwc_xport_t *pwc_xport_new_photon(const config_t *config, struct boot *boot,
                                  struct gas *gas)
  HPX_INTERNAL;

pwc_xport_t *pwc_xport_new(const config_t *config, struct boot *boot,
                           struct gas *gas)
  HPX_INTERNAL;

static inline void pwc_xport_delete(pwc_xport_t *xport) {
  xport->delete(xport);
}

#endif // LIBHPX_NETWORK_PWC_XPORT_H

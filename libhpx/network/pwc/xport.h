// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_NETWORK_PWC_XPORT_H
#define LIBHPX_NETWORK_PWC_XPORT_H

#include "Commands.h"
#include "libhpx/config.h"

extern "C" {
struct boot;
struct gas;
}

namespace libhpx {
namespace network {
namespace pwc {

#define XPORT_ANY_SOURCE -1
#define XPORT_KEY_SIZE   16

typedef char xport_key_t[XPORT_KEY_SIZE];

struct xport_op_t {
  unsigned        rank;
  int   UNUSED_PADDING;
  size_t             n;
  void           *dest;
  const void *dest_key;
  const void      *src;
  const void  *src_key;
  Command          lop;
  Command          rop;
} HPX_ALIGNED(HPX_CACHELINE_SIZE);

struct pwc_xport_t {
  libhpx_transport_t type;

  void (*dealloc)(void *xport);
  const void *(*key_find_ref)(void *xport, const void *addr, size_t n);
  void (*key_find)(void *xport, const void *addr, size_t n, void *key);
  void (*key_copy)(void * dest, const void * src);
  void (*key_clear)(void *key);
  int (*cmd)(int rank, Command lcmd, Command rcmd);
  int (*pwc)(xport_op_t *op);
  int (*gwc)(xport_op_t *op);
  int (*test)(Command *op, int *remaining, int id, int *src);
  int (*probe)(Command *op, int *remaining, int rank, int *src);
  void (*pin)(const void *base, size_t bytes, void *key);
  void (*unpin)(const void *base, size_t bytes);
  void (*create_comm)(void *comm, int rank, void* active_ranks, int num_active, int total);
  void (*allreduce)(void *sendbuf, void* out, int count, void* datatype, void* op, void* comm);
};

pwc_xport_t *pwc_xport_new_photon(const config_t *config, struct boot *boot,
                                  struct gas *gas);

pwc_xport_t *pwc_xport_new(const config_t *config, struct boot *boot,
                           struct gas *gas);

} // namespace pwc
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_PWC_XPORT_H

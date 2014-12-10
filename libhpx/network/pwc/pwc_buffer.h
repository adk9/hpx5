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
#ifndef LIBHPX_NETWORK_PWC_BUFFER_H
#define LIBHPX_NETWORK_PWC_BUFFER_H

#include <hpx/hpx.h>
#include <photon.h>

typedef struct {
  void         *rva;
  const void   *lva;
  size_t          n;
  hpx_addr_t  local;
  hpx_addr_t remote;
  hpx_action_t   op;
} pwc_record_t;

typedef struct {
  char                      *base;
  struct photon_buffer_priv_t key;
  uint32_t                   rank;
  uint32_t                   size;
  uint64_t                    min;
  uint64_t                    max;
  pwc_record_t           *records;
} pwc_buffer_t;

int pwc_buffer_init(pwc_buffer_t *buffer, uint32_t rank, uint32_t size)
  HPX_NON_NULL(1) HPX_INTERNAL;

void pwc_buffer_fini(pwc_buffer_t *buffer)
  HPX_NON_NULL(1) HPX_INTERNAL;

int pwc_buffer_pwc(pwc_buffer_t *buffer, void *rva, const void *lva, size_t n,
                   hpx_addr_t local, hpx_addr_t remote, hpx_action_t op)
  HPX_NON_NULL(1) HPX_INTERNAL;

int pwc_buffer_progress(pwc_buffer_t *buffer)
  HPX_NON_NULL(1);

#endif // LIBHPX_NETWORK_PWC_BUFFER_H

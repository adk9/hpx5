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
#ifndef LIBHPX_GAS_AGAS_H
#define LIBHPX_GAS_AGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include <libhpx/gas.h>

struct boot;
struct config;

typedef struct {
  gas_t vtable;
  size_t chunk_size;
  void *chunk_table;
  void *btt;
  void *bitmap;
  void *cyclic_bitmap;
  unsigned cyclic_arena;
} agas_t;

struct gas *gas_agas_new(const struct config *config, struct boot *boot)
  HPX_INTERNAL;

void agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync)
  HPX_INTERNAL;

int agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
                hpx_addr_t lsync, hpx_addr_t rsync)
  HPX_INTERNAL;

int agas_memget(void *gas, void *to, hpx_addr_t from, size_t n,
                hpx_addr_t lsync)
  HPX_INTERNAL;

int agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
                hpx_addr_t sync)
  HPX_INTERNAL;

#endif

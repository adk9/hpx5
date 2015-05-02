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

struct boot;
struct config;
struct gas;

struct gas *gas_agas_new(const struct config *config, struct boot *boot)
  HPX_INTERNAL;

int agas_memput(hpx_addr_t to, const void *from, size_t n, hpx_addr_t lsync,
                 hpx_addr_t rsync)
  HPX_INTERNAL;

int agas_memget(void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync)
  HPX_INTERNAL;

#endif

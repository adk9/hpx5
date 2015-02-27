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
#ifndef LIBHPX_GAS_PARCELS_EMULATION_H
#define LIBHPX_GAS_PARCELS_EMULATION_H

#include <hpx/attributes.h>

int parcel_memcpy(hpx_addr_t to, hpx_addr_t from, size_t size, hpx_addr_t sync)
  HPX_INTERNAL;

int parcel_memget(void *to, hpx_addr_t from, size_t size, hpx_addr_t lsync)
  HPX_INTERNAL;

int parcel_memput(hpx_addr_t to, const void *from, size_t size,
                  hpx_addr_t lsync, hpx_addr_t rsync)
  HPX_INTERNAL;

#endif // LIBHPX_GAS_PARCELS_EMULATION_H

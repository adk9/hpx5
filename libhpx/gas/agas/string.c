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

#include "agas.h"

void
agas_move(void *gas, hpx_addr_t src, hpx_addr_t dst, hpx_addr_t sync) {
}

int
agas_memput(void *gas, hpx_addr_t to, const void *from, size_t n,
            hpx_addr_t lsync, hpx_addr_t rsync) {
}

int
agas_memget(void *gas, void *to, hpx_addr_t from, size_t n, hpx_addr_t lsync) {
}

int
agas_memcpy(void *gas, hpx_addr_t to, hpx_addr_t from, size_t size,
            hpx_addr_t sync) {
}

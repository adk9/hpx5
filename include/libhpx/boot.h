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
#ifndef LIBHPX_BOOT_BOOT_H
#define LIBHPX_BOOT_BOOT_H

#include "hpx/attributes.h"

typedef struct boot boot_t;

HPX_INTERNAL boot_t *boot_new(void);
HPX_INTERNAL void boot_delete(boot_t*) HPX_NON_NULL(1);
HPX_INTERNAL int boot_rank(const boot_t*) HPX_NON_NULL(1);
HPX_INTERNAL int boot_n_ranks(const boot_t*) HPX_NON_NULL(1);

#endif // LIBHPX_BOOT_BOOT_H

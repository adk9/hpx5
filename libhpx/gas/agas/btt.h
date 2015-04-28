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
#ifndef LIBHPX_GAS_AGAS_BTT_H
#define LIBHPX_GAS_AGAS_BTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/hpx.h>
#include <hpx/attributes.h>

typedef struct {
  int64_t key;
  int32_t rank;
  int32_t count;
} entry_t;

HPX_INTERNAL void *btt_new(size_t size);
HPX_INTERNAL void btt_delete(void *btt);

HPX_INTERNAL entry_t *btt_find(void *btt, hpx_addr_t addr);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_BTT_H

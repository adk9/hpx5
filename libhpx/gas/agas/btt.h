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
#include "gva.h"

void *btt_new(size_t size);
void btt_delete(void *btt);

void btt_insert(void *btt, gva_t gva, int32_t owner, void *lva, size_t blocks);
void btt_remove(void *btt, gva_t gva);
void btt_try_delete(void *btt, gva_t gva, hpx_parcel_t *p);
bool btt_try_pin(void *btt, gva_t gva, void **lva);
void btt_unpin(void *btt, gva_t gva);
void *btt_lookup(const void* obj, gva_t gva);
uint32_t btt_owner_of(const void *btt, gva_t gva);
size_t btt_get_blocks(const void *btt, gva_t gva);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_BTT_H

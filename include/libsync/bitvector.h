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
#ifndef HPX_SYNC_BITVECTOR_H_
#define HPX_SYNC_BITVECTOR_H_

/// ----------------------------------------------------------------------------
/// @file include/libsync/bitvector.h
/// ----------------------------------------------------------------------------

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#include "sync.h"
#include "hpx/attributes.h"


typedef uint64_t _bitvec_word_t;

typedef struct {
  SYNC_ATOMIC(int size);
  _bitvec_word_t *word;
} bitvec_t;

// number of bits in a bitvec word
#define _bitvec_word_size  (sizeof(_bitvec_word_t) * CHAR_BIT)

// default size (in words) at initialization
#define _bitvec_default_size 8

bitvec_t *sync_bitvec_new(size_t size) HPX_MALLOC;
void sync_bitvec_delete(bitvec_t *b) HPX_NON_NULL(1);
void sync_bitvec_set(bitvec_t *b, int i) HPX_NON_NULL(1);
void sync_bitvec_unset(bitvec_t *b, int i) HPX_NON_NULL(1);
void sync_bitvec_toggle(bitvec_t *b, int i) HPX_NON_NULL(1);
bool sync_bitvec_test(bitvec_t *b, int i) HPX_NON_NULL(1);
void sync_bitvec_zero(bitvec_t *b) HPX_NON_NULL(1);
void sync_bitvec_fill(bitvec_t *b) HPX_NON_NULL(1);
    
#endif /* HPX_SYNC_BITVECTOR_H_ */

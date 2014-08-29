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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "libsync/bitvector.h"


int _bitvec_sizeof(bitvec_t *b) {
  assert(b);
  int size;
  sync_load(size, &b->size, SYNC_ACQUIRE);
  return size;
}


void _bitvec_bounds_check(bitvec_t *b, int word) {
  int size = _bitvec_sizeof(b);
  if (word > size) {
    _bitvec_word_t *bw = (_bitvec_word_t*)realloc(b->word, (size*2) * _bitvec_word_size);
    if (!sync_cas(&b->size, size, (size*2), SYNC_RELEASE, SYNC_RELAXED))
      _bitvec_bounds_check(b, word);
    b->word = bw;
  }
}


bitvec_t *sync_bitvec_new(size_t size) {
  // how many bitvec words do we need?
  int n = (size == 0) ? _bitvec_default_size : ((size + _bitvec_word_size - 1) / _bitvec_word_size);

  bitvec_t *b = malloc(sizeof(*b));
  if (b)
    sync_store(&b->size, n, SYNC_RELEASE);

  // allocate n bitvec words
  b->word = (_bitvec_word_t*)malloc(n * _bitvec_word_size);
  assert(b->word);
  return b;
}


void sync_bitvec_delete(bitvec_t *b) {
  if (!b)
    return;
  if (b->word)
    free(b->word);
  free(b);
}


void sync_bitvec_set(bitvec_t *b, int i) {
  int w = i/_bitvec_word_size;
  _bitvec_bounds_check(b, w);
  sync_for(&b->word[w], (1UL << (i % _bitvec_word_size)), SYNC_ACQ_REL);
}


void sync_bitvec_unset(bitvec_t *b, int i) {
  int w = i/_bitvec_word_size;
  _bitvec_bounds_check(b, w);
  sync_fand(&b->word[w], ~(1UL << (i % _bitvec_word_size)), SYNC_ACQ_REL);
}


void sync_bitvec_toggle(bitvec_t *b, int i) {
  int w = i/_bitvec_word_size;
  _bitvec_bounds_check(b, w);
  sync_fxor(&b->word[w], (1UL << (i % _bitvec_word_size)), SYNC_ACQ_REL);
}


bool sync_bitvec_test(bitvec_t *b, int i) {
  int w = i/_bitvec_word_size;
  _bitvec_bounds_check(b, w);
  _bitvec_word_t bw;
  sync_load(bw, &b->word[w], SYNC_ACQUIRE);
  return !!(bw & (1UL << (i % _bitvec_word_size)));
}


void sync_bitvec_zero(bitvec_t *b) {
  int size = _bitvec_sizeof(b);
  memset(b->word, 0, size);
}


void sync_bitvec_fill(bitvec_t *b) {
  int size = _bitvec_sizeof(b);
  memset(b->word, 0xFF, size);
}


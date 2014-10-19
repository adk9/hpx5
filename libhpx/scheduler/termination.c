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

#include "libsync/sync.h"
#include "libhpx/debug.h"
#include "termination.h"


void _bitmap_bounds_check(bitmap_t *b, uint32_t page, uint32_t word) {
  assert(word < _bitmap_num_words);

  // check if the page has been allocated or not
  _bitmap_page_t p = sync_load(&b[page].page, SYNC_ACQUIRE);
  if (!p) {
    _bitmap_page_t newp = (_bitmap_page_t)calloc(_bitmap_num_words, sizeof(_bitmap_word_t));
    if (!sync_cas(&b[page].page, p, newp, SYNC_RELEASE, SYNC_RELAXED)) {
      free((void*)newp);
      // try again..
      _bitmap_bounds_check(b, page, word);
    } else {
      // move on to the previous page
      _bitmap_bounds_check(b, page-1, word);
    }
  }
}


bitmap_t *cr_bitmap_new(void) {
  // allocate _bitmap_num_pages
  bitmap_t *b = (bitmap_t*)calloc(_bitmap_num_pages, sizeof(_bitmap_page_t));
  assert(b);

  // allocate the first page
  void *p = (_bitmap_page_t)calloc(_bitmap_num_words, sizeof(_bitmap_word_t));
  sync_store(&b[0].page, p, SYNC_RELEASE);
  return b;
}


void cr_bitmap_delete(bitmap_t *b) {
  if (!b)
    return;

  // delete all pages
  int i;
  for (i = 0; i < _bitmap_num_pages; ++i) {
    _bitmap_page_t p = sync_load(&b[i].page, SYNC_ACQUIRE);
    if (p)
      free((void*)p);
  }
  free(b);
}


void _bitmap_add_at(bitmap_t *b, uint32_t page, uint32_t word, uint32_t offset) {
  _bitmap_bounds_check(b, page, word);

  _bitmap_word_t old = sync_fadd(&(b[page].page[word]), (1UL << offset), SYNC_ACQ_REL);
  // if there was an overflow, add to the previous word
  if (old + (1UL << offset) < (1UL << offset)) {
    if (word == 0) {
      assert(page > 0);
      page--;
      word = _bitmap_num_words;
    }
    _bitmap_add_at(b, page, word-1, 0);
  }
}


void cr_bitmap_add(bitmap_t *b, int i) {
  uint32_t page = i/_bitmap_num_pages;
  uint32_t offset = i%_bitmap_page_size;
  uint32_t word = offset/_bitmap_word_size;

  int word_offset = _bitmap_word_size - (offset % _bitmap_word_size);
  _bitmap_add_at(b, page, word, word_offset);
}

// test if the bit at the 0th position (page 0 word 0) is set
bool cr_bitmap_test(bitmap_t *b) {
  _bitmap_word_t w = sync_load(&b[0].page[0], SYNC_ACQUIRE);
  return (w == (1UL << 63));
}


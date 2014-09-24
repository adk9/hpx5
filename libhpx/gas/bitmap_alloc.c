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
# include "config.h"
#endif

/// @file libhpx/gas/bitmap_alloc.c
/// @brief Implement a simple parallel bitmap.

#include <assert.h>
#include <string.h>
#include <hpx/builtins.h>
#include <libsync/sync.h>
#include "libhpx/libhpx.h"
#include "bitmap_alloc.h"

// The number of bits in a word.
const uint32_t BITS_PER_WORD = sizeof(uintptr_t) * 8;

size_t bitmap_alloc_sizeof(uint32_t n) {
  return sizeof(bitmap_alloc_t) + n / 8;
}

void bitmap_alloc_init(bitmap_alloc_t *bitmap, const uint32_t n) {
  sync_tatas_init(&bitmap->lock);
  bitmap->min = 0;
  memset(&bitmap->bits, 0xFF, bitmap_alloc_sizeof(n));
}

static inline uint32_t sat_sub32(const uint32_t lhs, const uint32_t rhs) {
  return (lhs < rhs) ? 0 : lhs - rhs;
}

static inline uint32_t min32(const uint32_t lhs, const uint32_t rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

/// Search for a contiguous region of @p n free bits in the @p bits bitmap.
///
/// @param   words The bitmap.
/// @param    word The word offset into @p bits where the search starts.
/// @param  offset The offset into @p word where the search starts.
/// @param       n The number of contiguous bits we need to find.
/// @param   total The total number of bits we've already searched.
///
/// @returns The @p off-relative offset where we found a region of free bits. If
///          we return 0, this means that @p off was the start of the
///          allocation, otherwise the start was @p off + the return value.
///
static uint32_t search(uintptr_t *words, uint32_t word, uint32_t offset,
                       const uint32_t n, const uint32_t total) {
  uint32_t remaining = n;

  while (remaining > 0) {
    const uintptr_t rshift = sat_sub32(BITS_PER_WORD - offset, n);
    const uintptr_t mask = (UINTPTR_MAX << offset) >> rshift;
    const uintptr_t bits = mask & words[word];

    if (bits != mask) {
      // We mismatched, restart from the most significant bit of the mismatch,
      // keeping track of the total number of bits we've searched so far.
      const uint32_t msb = BITS_PER_WORD - clzl(mask ^ bits);
      const uint32_t next_word = word + msb / BITS_PER_WORD;
      const uint32_t next_offset = msb % BITS_PER_WORD;
      const uint32_t next_total = total + (n - remaining) + (msb - offset);
      return search(words, next_word, next_offset, n, next_total);
    }

    remaining -= popcountl(bits);
    ++word;
    offset = 0;
  }

  return total;
}

static void set(uintptr_t *words, const uint32_t from, uint32_t n) {
  uint32_t word = from / BITS_PER_WORD;
  uint32_t offset = from % BITS_PER_WORD;

  while (n > 0) {
    const uintptr_t rshift = sat_sub32(BITS_PER_WORD - offset, n);
    const uintptr_t mask = (UINTPTR_MAX << offset) >> rshift;
    words[word] |= mask;
    n -= popcountl(mask);
    ++word;
    offset = 0;
  }
}

static void reset(uintptr_t *words, uint32_t from, uint32_t n) {
  uint32_t word = from / BITS_PER_WORD;
  uint32_t offset = from % BITS_PER_WORD;

  while (n > 0) {
    const uintptr_t rshift = sat_sub32(BITS_PER_WORD - offset, n);
    const uintptr_t mask = (UINTPTR_MAX << offset) >> rshift;
    words[word] &= ~mask;
    n -= popcountl(mask);
    ++word;
    offset = 0;
  }
}

int bitmap_alloc_alloc(bitmap_alloc_t *bitmap, const uint32_t n, uint32_t *i) {
  int status;
  if (n == 0)
    return LIBHPX_EINVAL;

  status = LIBHPX_OK;
  sync_tatas_acquire(&bitmap->lock);

  // Find the first potential offset, using the cached min word.
  const uintptr_t start = bitmap->bits[bitmap->min];
  assert(start);

  // we're computing the absolute offset *i as
  //   abs: #bits to get to the min word
  //  woff: #bits offset into the min word we begin
  //  roff: #bits relative to offset we found space
  const uint32_t offset = ctzl(start);
  const uint32_t relative = search(bitmap->bits, start, offset, n, 0);
  const uint32_t abs = bitmap->min * BITS_PER_WORD + offset + relative;
  const uint32_t end = abs + n;

  // update our min word, ensuring the invariant that it always points to a word
  // with at least one free bit
  bitmap->min = end / BITS_PER_WORD;
  while (!bitmap->bits[bitmap->min])
    ++bitmap->min;

  // set the bits
  set(bitmap->bits, abs, n);

  // output the absolute total start of the allocation
  *i = abs;

  sync_tatas_release(&bitmap->lock);
  return status;
}

void bitmap_alloc_free(bitmap_alloc_t *bitmap, const uint32_t from,
                       const uint32_t n) {
  sync_tatas_acquire(&bitmap->lock);
  reset(bitmap->bits, from, n);
  bitmap->min = min32(bitmap->min, from / BITS_PER_WORD);
  sync_tatas_release(&bitmap->lock);
}

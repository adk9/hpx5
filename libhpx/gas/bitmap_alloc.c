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
const uint32_t W = sizeof(uintptr_t) * 8;

size_t bitmap_alloc_sizeof(uint32_t n) {
  return n / 8;
}

void bitmap_alloc_init(bitmap_alloc_t *bitmap, uint32_t n) {
  sync_tatas_init(&bitmap->lock);
  bitmap->min = 0;
  memset(&bitmap->bits, 0xFF, bitmap_alloc_sizeof(n));
}

static uint32_t saturating_sub(uint32_t lhs, uint32_t rhs) {
  return (lhs < rhs) ? 0 : lhs - rhs;
}

/// Search for a contiguous region of @p n free bits in the @p bits bitmap.
///
/// @param    bits The bitmap.
/// @param       w The offset into @p bits where the search starts.
/// @param    woff The offset into @p bits[@p start] where the search starts.
/// @param       n The number of contiguous bits we need to find.
/// @param   total The total number of bits we've already searched.
///
/// @returns The @p off-relative offset where we found a region of free bits. If
///          we return 0, this means that @p off was the start of the
///          allocation, otherwise the start was @p off + the return value.
///
static uint32_t search(uintptr_t *words, uint32_t w, uint32_t woff,
                       uint32_t n, uint32_t total) {
  uint32_t remaining = n;

  while (remaining > 0) {
    uintptr_t mask = (UINTPTR_MAX << woff) >> saturating_sub(W - woff, n);
    uintptr_t bits = mask & words[w];

    if (bits != mask) {
      // We mismatched, restart from the most significant bit of the mismatch,
      // keeping track of the total number of bits we've searched so far.
      uint32_t msb = W - clzl(mask ^ bits);
      total = total + (n - remaining) + (msb - woff);
      w = w + msb / W;
      woff = msb % W;
      return search(words, w, woff, n, total);
    }

    remaining -= popcountl(bits);
    w = w + 1;
    woff = 0;
  }

  return total;
}

static void set(uintptr_t *words, uint32_t start, uint32_t n) {
  uint32_t w = n / W;
  uint32_t woff = n % W;

  while (n > 0) {
    uintptr_t mask = (UINTPTR_MAX << woff) >> saturating_sub(W - woff, n);
    words[w] |= mask;
  }
}

static void reset(uintptr_t *words, uint32_t start, uint32_t n) {
  uint32_t w = n / W;
  uint32_t woff = n % W;

  while (n > 0) {
    uintptr_t mask = (UINTPTR_MAX << woff) >> saturating_sub(W - woff, n);
    words[w] |= ~mask;
  }
}

int bitmap_alloc_alloc(bitmap_alloc_t *bitmap, uint32_t n, uint32_t *i) {
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
  //  woff: #bits into the min word we begin
  //  roff: #bits relative to woff we found space
  const uint32_t woff = ctzl(start);
  const uint32_t roff = search(bitmap->bits, start, woff, n, 0);
  const uint32_t abs = bitmap->min * W + woff + roff;
  const uint32_t end = abs + n;

  // update our min word, ensuring the invariant that it always points to a word
  // with at least one free bit
  bitmap->min = end / sizeof(start);
  while (!bitmap->bits[bitmap->min])
    ++bitmap->min;

  // set the bits
  set(bitmap->bits, abs, n);

  // output the absolute total start of the allocation
  *i = abs;

  sync_tatas_release(&bitmap->lock);
  return status;
}

void bitmap_alloc_free(bitmap_alloc_t *bitmap, uint32_t i, uint32_t n) {
  sync_tatas_acquire(&bitmap->lock);
  reset(bitmap->bits, i, n);
  sync_tatas_release(&bitmap->lock);
}

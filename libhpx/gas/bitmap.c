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

/// @file libhpx/gas/bitmap.c
/// @brief Implement a simple parallel bitmap.

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <hpx/builtins.h>
#include <libsync/sync.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "bitmap.h"

// The number of bits in a word.
const uint32_t BITS_PER_WORD = sizeof(uintptr_t) * 8;

size_t bitmap_sizeof(const uint32_t n) {
  return sizeof(bitmap_t) + n / 8;
}

void bitmap_init(bitmap_t *bitmap, const uint32_t n) {
  sync_tatas_init(&bitmap->lock);
  bitmap->min = 0;
  const uintptr_t pattern = UINTPTR_MAX;
  memset(&bitmap->bits, pattern, bitmap_sizeof(n));
}

bitmap_t *bitmap_new(const uint32_t n) {
  const size_t bytes = bitmap_sizeof(n);
  bitmap_t *bitmap = malloc(bytes);
  if (!bitmap)
    dbg_error("bitmap: failed to allocate a bitmap for %u blocks\n", n);
  bitmap_init(bitmap, n);
  return bitmap;
}

void bitmap_delete(bitmap_t *bitmap) {
  if (!bitmap)
    return;

  free(bitmap);
}

static inline uint32_t _sat_sub32(const uint32_t lhs, const uint32_t rhs) {
  return (lhs < rhs) ? 0 : lhs - rhs;
}

static inline uint32_t _min32(const uint32_t lhs, const uint32_t rhs) {
  return (lhs < rhs) ? lhs : rhs;
}

/// Search for a contiguous region of @p n free bits in the @p bits bitmap.
///
/// @param   words The bitmap.
/// @param    word The word offset into @p bits where the search starts.
/// @param  offset The offset into @p word where the search starts.
/// @param       n The number of contiguous bits we need to find.
/// @param   align The alignment requirements for the allocation.
/// @param   total The total number of bits we've already searched.
///
/// @returns The @p off-relative offset where we found a region of free bits. If
///          we return 0, this means that @p off was the start of the
///          allocation, otherwise the start was @p off + the return value.
///
static uint32_t _search(uintptr_t *words, uint32_t word, uint32_t offset,
                        const uint32_t n, const uint32_t align,
                        const uint32_t total) {
  // make sure that we start with a good alignment
  const uint32_t r = offset % align;
  if (offset % align)
    return _search(words, word, offset + r, n, align, total + r);

  // scan for enough bytes
  uint32_t remaining = n;

  while (remaining > 0) {
    const uintptr_t rshift = _sat_sub32(BITS_PER_WORD, n);
    const uintptr_t mask = (UINTPTR_MAX >> rshift) << offset;
    const uintptr_t bits = mask & words[word];

    if (bits != mask) {
      // We mismatched, restart from the most significant bit of the mismatch,
      // keeping track of the total number of bits we've searched so far.
      const uint32_t msb = BITS_PER_WORD - clzl(mask ^ bits);
      const uint32_t next_word = word + msb / BITS_PER_WORD;
      const uint32_t next_offset = msb % BITS_PER_WORD;
      const uint32_t next_total = total + (n - remaining) + (msb - offset);
      return _search(words, next_word, next_offset, n, align, next_total);
    }

    remaining -= popcountl(bits);
    ++word;
    offset = 0;
  }

  // found a matching allocation
  return total;
}

/// Set a chunk of contiguous space in the bitmap to 0.
static void _set(uintptr_t *words, const uint32_t from, uint32_t n) {
  uint32_t word = from / BITS_PER_WORD;
  uint32_t offset = from % BITS_PER_WORD;

  while (n > 0) {
    const uintptr_t rshift = _sat_sub32(BITS_PER_WORD, n);
    const uintptr_t mask = (UINTPTR_MAX >> rshift) << offset;

    //   word   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 0 0 1

    words[word] &= ~mask;
    n -= popcountl(mask);
    ++word;
    offset = 0;
  }
}

/// Set a chunk of contiguous space in the bitmap to 1.
static void _reset(uintptr_t *words, uint32_t from, uint32_t n) {
  uint32_t word = from / BITS_PER_WORD;
  uint32_t offset = from % BITS_PER_WORD;

  while (n > 0) {
    const uintptr_t rshift = _sat_sub32(BITS_PER_WORD, n);
    const uintptr_t mask = (UINTPTR_MAX >> rshift) << offset;

    //   word   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 1 1 1

    words[word] |= mask;
    n -= popcountl(mask);
    ++word;
    offset = 0;
  }
}

int bitmap_reserve(bitmap_t *bitmap, const uint32_t n, const uint32_t align,
                   uint32_t *i) {
  dbg_log_gas("bitmap: searching for %u blocks with %u alignment.\n", n, align);
  int status;
  if (n == 0)
    return LIBHPX_EINVAL;

  status = LIBHPX_OK;
  sync_tatas_acquire(&bitmap->lock);

  // Find the first potential offset, using the cached min word.
  const uint32_t min = bitmap->min;
  const uintptr_t start = bitmap->bits[min];
  assert(start);

  // we're computing the absolute offset *i as
  //   abs: #bits to get to the min word
  //  woff: #bits offset into the min word we begin
  //  roff: #bits relative to offset we found space
  const uint32_t offset = ctzl(start);
  const uint32_t relative = _search(bitmap->bits, min, offset, n, align, 0);
  const uint32_t abs = bitmap->min * BITS_PER_WORD + offset + relative;
  const uint32_t end = abs + n;
  assert(abs % align == 0);

  // set the bits
  _set(bitmap->bits, abs, n);

  // update our min word, ensuring the invariant that it always points to a word
  // with at least one free bit
  if (relative == 0) {
    bitmap->min = end / BITS_PER_WORD;
    while (!bitmap->bits[bitmap->min]) {
      ++bitmap->min;
      dbg_log_gas("updated min to %u, for word %p.\n",
                  bitmap->min, (void*)bitmap->bits[bitmap->min]);
    }
  }

  assert(bitmap->bits[bitmap->min]);

  // output the absolute total start of the allocation
  *i = abs;

  sync_tatas_release(&bitmap->lock);
  dbg_log_gas("bitmap: found at offset %u.\n", abs);
  return status;
}

void bitmap_release(bitmap_t *bitmap, const uint32_t from, const uint32_t n) {
  dbg_log_gas("bitmap: release %u blocks at %u.\n", n, from);
  sync_tatas_acquire(&bitmap->lock);
  _reset(bitmap->bits, from, n);
  uint32_t min = _min32(bitmap->min, from / BITS_PER_WORD);
  if (min != bitmap->min)
    dbg_log_gas("updated min from %u to %u, for word %p.\n",
                bitmap->min, min, (void*)bitmap->bits[bitmap->min]);
  bitmap->min = min;
  assert(bitmap->bits[bitmap->min]);
  sync_tatas_release(&bitmap->lock);
}

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

typedef uintptr_t block_t;

static const block_t   BLOCK_ALL_FREE = UINTPTR_MAX;
static const uint32_t BYTES_PER_BLOCK = sizeof(block_t);
static const uint32_t  BITS_PER_BLOCK = sizeof(block_t) * 8;

static inline uint32_t block_clz(block_t b) {
  assert(b);
  return clzl(b);
}

static inline uint32_t block_ctz(block_t b) {
  assert(b);
  return ctzl(b);
}

static inline uint32_t block_popcount(block_t b) {
  return popcountl(b);
}

/// Perform a saturating 32-bit subtraction.
static inline uint32_t _sat_sub32(uint32_t lhs, uint32_t rhs) {
  return (lhs < rhs) ? 0 : lhs - rhs;
}


/// Perform a 32-bit minimum operations.
static inline uint32_t _min32(uint32_t lhs, uint32_t rhs) {
  return (lhs < rhs) ? lhs : rhs;
}


static inline block_t _create_mask(uint32_t offset, uint32_t length) {
  // We want to mask set to ones from offset that can either satisfy length, or
  // goes to the end of the block boundary.
  //
  // before: mask == (1111 ... 1111)
  //  after: mask == (0011 ... 1000) for offset == 3
  //                                 and length == (BITS_PER_BLOCK - 5)
  //
  // rshift: the number of most significant zeros we will initially add
  //
  const uint32_t rshift = _sat_sub32(BITS_PER_BLOCK, length);

  // Shifting BLOCK_ALL_FREE right first creates the maximum length string of
  // 1s that we might match in this block, then shifting left might push some
  // of those bits off the end. Regardless, the resulting mask has the correct
  // bits set
  //
  return (BLOCK_ALL_FREE >> rshift) << offset;
}

struct bitmap {
  tatas_lock_t      lock;
  uint32_t     min_block;
  uint32_t         nbits;
  uint32_t       nblocks;
  block_t       blocks[];
};


/// Search for a contiguous region of @p n free bits in the @p bits bitmap.
///
/// @param     map The bitmap.
/// @param   nbits The number of contiguous bits we need to find.
/// @param   align The alignment requirements for the allocation.
/// @param   block The block offset into @p bits where the search starts.
/// @param     bit The bit offset into (@p map)->blocks[(@p b)] where the search
///                starts.
/// @param   accum The total number of bits we've already searched.
///
/// @returns The absolute offset where we found a region of free bits.
static uint32_t _search(bitmap_t *map,
                        const uint32_t nbits, const uint32_t align,
                        uint32_t block, uint32_t bit, uint32_t accum) {

  // make sure that we start with a good alignment
  const uint32_t r = bit % align;
  if (r) {
    // we have a bad alignment, shift the search and restart
    const uint32_t shift = align - r;
    block += (bit + shift) / BITS_PER_BLOCK;
    bit   += (bit + shift) % BITS_PER_BLOCK;
    accum += shift;
    assert(bit % align == 0);
    return _search(map, nbits, align, block, bit, accum);
  }

  // scan for enough bits at this location
  uint32_t remaining = nbits;
  while (remaining > 0) {
    const block_t mask = _create_mask(bit, remaining);
    const block_t bits = mask & map->blocks[block];

    if (bits != mask) {
      // If we mismatch, restart from the most significant bit of the mismatch,
      // keeping track of the total number of bits we've searched so far.
      //
      //  leading: the number of leading zeros before we get our mismatch
      //      msm: the index of the most significant mismatch
      const uint32_t leading = block_clz(mask ^ bits);
      const uint32_t     msm = BITS_PER_BLOCK - leading;

      // Update the number of accumulated bits we're skipping based on the
      // number of bits that we've successfully matched in the loop (nbits -
      // remaining), as well as the number of bits we're skipping in this block
      // [bit, msm).
      //
      accum += (nbits - remaining) + (msm - bit);

      // If the mismatch is in the block's most significant bit, then we need to
      // roll over into the next block for the restart, otherwise we just leave
      // the block alone and update the current bit offset to the msm.
      //
      block += (leading) ? 0 : 1;
      bit    = (leading) ? 0 : msm;

      // restart the search
      return _search(map, nbits, align, block, bit, accum);
    }
    else {
      // update the number of bits remaining to match, along with the current
      // block index (we're doing this loop per-block), and after the first
      // block the bit offset should be 0
      remaining -= block_popcount(bits);
      ++block;
      bit = 0;
    }
  }

  // found a matching allocation
  return accum;
}


/// Set a contiguous number of bits in the bitmap.
///
/// @param          map The bitmap we're updating.
/// @param            i The absolute bit index we're starting with.
/// @param        nbits The number of bits that we're setting.
static void _set(bitmap_t *map, uint32_t i, uint32_t nbits) {
  uint32_t block = i / BITS_PER_BLOCK;
  uint32_t   bit = i % BITS_PER_BLOCK;

  while (nbits > 0) {
    // ok to read naked because we're holding the lock and thus ordered with all
    // other writers
    const block_t  val = map->blocks[block];
    const block_t mask = _create_mask(bit, nbits);

    //    val   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 0 0 1

    // synchronized write so that it can be read non-blockingly in is_set, the
    // write doesn't have to serve as a releasing write though, it just needs to
    // not race w.r.t. the read in is_set, so we use relaxed ordering
    sync_store(&map->blocks[block], val & ~mask, SYNC_RELAXED);

    // decrement the number of bits left to deal with, move to the next block,
    // and reset the bit offset to the first bit in the next block (always safe)
    nbits -= block_popcount(mask);
    ++block;
    bit = 0;
  }
}


/// Clear a continuous number of bits in the bitmap.
///
/// @param          map The bitmap we're updating.
/// @param            i The absolute bit index we're starting with.
/// @param        nbits The number of bits that we're clearing.
static void _clear(bitmap_t *map, uint32_t i, uint32_t nbits) {
  uint32_t block = i / BITS_PER_BLOCK;
  uint32_t   bit = i % BITS_PER_BLOCK;

  while (nbits > 0) {
    // ok to read naked because we're holding the lock and thus ordered with all
    // other writers
    const block_t  val = map->blocks[block];
    const block_t mask = _create_mask(bit, nbits);

    //    val   0 0 1 1
    //   mask   0 1 1 0
    //          -------
    // result   0 1 1 1


    // synchronized write so that it can be read non-blockingly in is_set, the
    // write doesn't have to serve as a releasing write though, it just needs to
    // not race w.r.t. the read in is_set, so we use relaxed ordering
    sync_store(&map->blocks[block], val | mask, SYNC_RELAXED);

    // decrement the number of bits left to deal with, move to the next block,
    // and reset the bit offset to the first bit in the next block (always safe)
    nbits -= block_popcount(mask);
    ++block;
    bit = 0;
  }
}


bitmap_t *bitmap_new(uint32_t nbits) {
  uint32_t nblocks = ceil_div_32(nbits, BITS_PER_BLOCK);
  bitmap_t *map = malloc(sizeof(*map) + nblocks * BYTES_PER_BLOCK);
  if (!map)
    dbg_error("bitmap: failed to allocate a bitmap for %u bits\n", nbits);

  sync_tatas_init(&map->lock);
  map->min_block = 0;
  map->nbits     = nbits;
  map->nblocks   = nblocks;
  memset(&map->blocks, BLOCK_ALL_FREE, nblocks * BYTES_PER_BLOCK);
  return map;
}


void bitmap_delete(bitmap_t *map) {
  if (map)
    free(map);
}


int bitmap_reserve(bitmap_t *map, uint32_t nbits, uint32_t align, uint32_t *i) {
  dbg_log_gas("bitmap: searching for %u blocks with %u alignment.\n", nbits,
              align);
  int status;
  if (nbits == 0)
    return LIBHPX_EINVAL;

  status = LIBHPX_OK;
  sync_tatas_acquire(&map->lock);

  // Find the first potential bit offset, using the cached min block.
  const uint32_t    block = map->min_block;
  const block_t     start = map->blocks[block];
  const uint32_t      bit = block_ctz(start);
  const uint32_t relative = _search(map, nbits, align, block, bit, 0);
  const uint32_t      abs = map->min_block * BITS_PER_BLOCK + bit + relative;
  const uint32_t      end = abs + nbits;

  assert(abs % align == 0);
  if (end >= map->nbits) {
    dbg_error("application ran out of global address space. This space is used\n"
              "for all global allocation, as well as all stacks and parcel\n"
              "data. Try adjusting your configuration.\n");
  }

  // set the bits
  _set(map, abs, nbits);

  // update our min block, ensuring the invariant that it always points to a
  // block with at least one free bit
  if (relative == 0) {
    map->min_block = end / BITS_PER_BLOCK;
    while (!map->blocks[map->min_block]) {
      ++map->min_block;
      dbg_log_gas("updated min to %u, for blocks %p.\n", map->min_block,
                  (void*)map->blocks[map->min_block]);
    }
  }

  // output the absolute total start of the allocation
  *i = abs;

  sync_tatas_release(&map->lock);
  dbg_log_gas("bitmap: found at offset %u.\n", abs);
  return status;
}


void bitmap_release(bitmap_t *map, uint32_t i, uint32_t nbits) {
  dbg_log_gas("bitmap: release %u blocks at %u.\n", nbits, i);
  sync_tatas_acquire(&map->lock);
  _clear(map, i, nbits);
  uint32_t min = _min32(map->min_block, i / BITS_PER_BLOCK);
  if (min != map->min_block)
    dbg_log_gas("updated min from %u to %u, for block %p.\n",
                map->min_block, min, (void*)map->blocks[map->min_block]);
  map->min_block = min;
  assert(map->blocks[map->min_block]);
  sync_tatas_release(&map->lock);
}


bool bitmap_is_set(bitmap_t *map, uint32_t bit) {
  const uint32_t i = bit / BITS_PER_BLOCK;
  const uint32_t r = bit % BITS_PER_BLOCK;
  const block_t block = sync_load(&map->blocks[i], SYNC_RELAXED);
  const block_t mask = ((block_t)1) << r;

  // block:  0 0 1 1     ~block:  1 1 0 0
  //  mask:  1 0 1 0       mask:  1 0 1 0
  // -------------
  //  val  1 0 0 0

  const block_t val = ~block & mask;
  return val != 0;
}

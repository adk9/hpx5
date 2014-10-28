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

#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include "heap.h"
#include "gva.h"
#include "pgas.h"

/// Compute the phase (offset within block) of a global address.
static uint32_t _phase_of(hpx_addr_t gva, uint32_t bsize) {
  // the phase is stored in the least significant bits of the gva, we merely
  // mask it out
  //
  // before: (locality, offset, phase)
  //  after: (00000000  000000  phase)
  const uint64_t mask = ceil_log2_32(bsize) - 1;
  return (uint32_t)(gva & mask);
}


/// Compute the block ID for a global address.
static uint64_t _block_of(hpx_addr_t gva, uint32_t bsize) {
  // clear the upper bits by shifting them out, and then shifting the offset
  // down to the right place
  //
  // before: (locality, block, phase)
  //  after: (00000000000      block)
  const uint32_t ranks = here->ranks;
  const uint32_t lshift = ceil_log2_32(ranks);
  const uint32_t rshift = ceil_log2_32(bsize) + lshift;
  return (gva << lshift) >> rshift;
}


static hpx_addr_t _triple_to_gva(uint32_t rank, uint64_t bid, uint32_t phase,
                                 uint32_t bsize) {
  // make sure that the phase is in the expected range, locality will be checked
  DEBUG_IF (bsize && phase) {
    if (phase >= bsize) {
      dbg_error("phase %u must be less than %u\n", phase, bsize);
    }
  }

  DEBUG_IF (!bsize && phase) {
    dbg_error("cannot initialize a non-cyclic gva with a phase of %u\n", phase);
  }

  // forward to pgas_offset_to_gva(), by computing the offset by combining bid
  // and phase
  const uint32_t shift = (bsize) ? ceil_log2_32(bsize) : 0;
  const uint64_t offset = (bid << shift) + phase;
  return pgas_offset_to_gva(rank, offset);
}

int64_t pgas_gva_sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  // for a block cyclic computation, we look at the three components
  // separately, and combine them to get the overall offset
  const uint32_t plhs = _phase_of(lhs, bsize);
  const uint32_t prhs = _phase_of(rhs, bsize);
  const uint32_t llhs = pgas_gva_to_rank(lhs);
  const uint32_t lrhs = pgas_gva_to_rank(rhs);
  const uint32_t blhs = _block_of(lhs, bsize);
  const uint32_t brhs = _block_of(rhs, bsize);

  const int32_t dphase = plhs - prhs;
  const int32_t dlocality = llhs - lrhs;
  const int64_t dblock = blhs - brhs;

  // each difference in the phase is just one byte,
  // each difference in the locality is bsize bytes, and
  // each difference in the phase is entire cycle of bsize bytes
  const int64_t d = dblock * here->ranks * bsize + dlocality * bsize + dphase;

  // make sure we're not crazy
  DEBUG_IF (pgas_gva_add_cyclic(lhs, d, bsize) != rhs) {
    dbg_error("difference between %lu and %lu computed incorrectly as %ld\n",
              lhs, rhs, d);
  }
  return d;
}

/// Perform address arithmetic on a PGAS global address.
hpx_addr_t pgas_gva_add_cyclic(hpx_addr_t gva, int64_t bytes, uint32_t bsize) {
  if (!bsize)
    return gva + bytes;

  const uint32_t phase = (_phase_of(gva, bsize) + bytes) % bsize;
  const uint32_t blocks = (_phase_of(gva, bsize) + bytes) / bsize;
  const uint32_t rank = (pgas_gva_to_rank(gva) + blocks) % here->ranks;
  const uint32_t cycles = (pgas_gva_to_rank(gva) + blocks) / here->ranks;
  const uint64_t block = _block_of(gva, bsize) + cycles;

  const hpx_addr_t addr = _triple_to_gva(rank, block, phase, bsize);

  // sanity check
  DEBUG_IF (true) {
    const void *lva = pgas_gva_to_lva(addr);
    if (!heap_contains_lva(global_heap, lva))
      dbg_error("computed out of bounds address\n");
  }

  return addr;
}

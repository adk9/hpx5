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
#include "libhpx/debug.h"
#include "heap.h"
#include "gva.h"
#include "pgas.h"

uint32_t pgas_gva_locality_of(pgas_gva_t gva, uint32_t ranks) {
  // the locality is stored in the most significant bits of the gva, we just
  // need to shift it down correctly
  //
  // before: (locality, offset, phase)
  //  after: (00000000000000 locality)
  const uint32_t rshift = (sizeof(pgas_gva_t) * 8) - ceil_log2_32(ranks);
  return (uint32_t)(gva >> rshift);
}

uint64_t pgas_gva_offset_of(pgas_gva_t gva, uint32_t ranks, uint32_t bsize) {
  // clear the upper bits by shifting them out, and then shifting the offset
  // down to the right place
  //
  // before: (locality, offset, phase)
  //  after: (00000000000      offset)
  const uint32_t lshift = ceil_log2_32(ranks);
  const uint32_t rshift = ceil_log2_32(bsize) + lshift;
  return (gva << lshift) >> rshift;
}

uint64_t pgas_gva_heap_offset_of(pgas_gva_t gva, uint32_t ranks) {
  // the heap offset is just the least significant chunk of the gva, we shift
  // the locality out rather than masking because it's easier for us to express
  //
  // before: (locality, heap_offset)
  //  after: (00000000  heap_offset)
  const uint32_t shift = ceil_log2_32(ranks);
  return (gva << shift) >> shift;
}

uint32_t pgas_gva_phase_of(pgas_gva_t gva, uint32_t bsize) {
  // the phase is stored in the least significant bits of the gva, we merely
  // mask it out
  //
  // before: (locality, offset, phase)
  //  after: (00000000  000000  phase)
  const uint64_t mask = ceil_log2_32(bsize) - 1;
  return (uint32_t)(gva & mask);
}

pgas_gva_t pgas_gva_from_heap_offset(uint32_t locality, uint64_t heap_offset,
                                     uint32_t ranks) {
  // make sure the locality is in the expected range
  DEBUG_IF (ranks < locality) {
    assert(ranks > 0);
    dbg_error("locality %u must be less than %u\n", locality,
              pgas_fit_log2_32(ranks));
  }

  // make sure that the heap offset is in the expected range (everyone has the
  // same size, so the locality is irrelevant here)
  DEBUG_IF (!heap_offset_inbounds(global_heap, heap_offset)) {
    dbg_error("heap offset %lu is out of range\n", heap_offset);
  }

  // construct the gva by shifting the locality into the most significant bits
  // of the gva and then adding in the heap offset
  const uint32_t shift = (sizeof(pgas_gva_t) * 8) - ceil_log2_32(ranks);
  const uint64_t high = ((uint64_t)locality) << shift;
  return high + heap_offset;
}

pgas_gva_t pgas_gva_from_hpx_addr(hpx_addr_t addr) {
  return (pgas_gva_t)addr;
}

hpx_addr_t pgas_gva_to_hpx_addr(pgas_gva_t gva) {
  const hpx_addr_t addr = (hpx_addr_t)gva;
  return addr;
}


pgas_gva_t pgas_gva_from_triple(uint32_t locality, uint64_t offset,
                                uint32_t phase, uint32_t ranks, uint32_t bsize)
{
  // make sure that the phase is in the expected range, locality will be checked
  DEBUG_IF (bsize && phase) {
    if (ceil_log2_32(bsize) < ceil_log2_32(phase)) {
      dbg_error("phase %u must be less than %u\n", phase,
                pgas_fit_log2_32(bsize));
    }
  }

  DEBUG_IF (!bsize && phase) {
    dbg_error("cannot initialize a non-cyclic gva with a phase of %u\n", phase);
  }

  // forward to the heap_offset version, by computing the heap offset through a
  // shift of the gva offset
  const uint32_t shift = (bsize) ? ceil_log2_32(bsize) : 0;

  return pgas_gva_from_heap_offset(locality, (offset << shift) + phase, ranks);
}



int64_t pgas_gva_sub(pgas_gva_t lhs, pgas_gva_t rhs, uint32_t ranks,
                     uint32_t bsize)
{
  if (!bsize) {
    // if bsize is zero then this should be a local computation, check to make
    // sure the localities match, and then just compute the
    // heap_offset_difference.
    const uint32_t llhs = pgas_gva_locality_of(lhs, ranks);
    const uint32_t lrhs = pgas_gva_locality_of(rhs, ranks);
    DEBUG_IF (lrhs != llhs) {
      dbg_error("local arithmetic failed between %u and %u\n", llhs, lrhs);
    }

    return lhs - rhs;
  }
  else {
    // for a block cyclic computation, we look at the three components
    // separately, and combine them to get the overall offset
    const uint32_t plhs = pgas_gva_phase_of(lhs, bsize);
    const uint32_t prhs = pgas_gva_phase_of(rhs, bsize);
    const uint32_t llhs = pgas_gva_locality_of(lhs, bsize);
    const uint32_t lrhs = pgas_gva_locality_of(rhs, bsize);
    const uint32_t olhs = pgas_gva_offset_of(lhs, ranks, bsize);
    const uint32_t orhs = pgas_gva_offset_of(rhs, ranks, bsize);

    const int32_t dphase = plhs - prhs;
    const int32_t dlocality = llhs - lrhs;
    const int64_t doffset = olhs - orhs;

    // each difference in the phase is just one byte,
    // each difference in the locality is bsize bytes, and
    // each difference in the phase is entire cycle of bsize bytes
    const int64_t dist = doffset * ranks * bsize + dlocality * bsize + dphase;

    // make sure we're not crazy
    DEBUG_IF (pgas_gva_add_cyclic(lhs, dist, ranks, bsize) != rhs) {
      dbg_error("difference between %lu and %lu computed incorrectly as %ld\n",
                lhs, rhs, dist);
    }
    return dist;
  }
}

/// Perform address arithmetic on a PGAS global address.
pgas_gva_t pgas_gva_add_cyclic(pgas_gva_t gva, int64_t bytes, uint32_t ranks,
                               uint32_t bsize) {
  if (!bsize)
    return gva + bytes;

  const uint32_t phase = (pgas_gva_phase_of(gva, bsize) + bytes) % bsize;
  const uint32_t blocks = (pgas_gva_phase_of(gva, bsize) + bytes) / bsize;
  const uint32_t locality = (pgas_gva_locality_of(gva, ranks) + blocks) % ranks;
  const uint32_t cycles = (pgas_gva_locality_of(gva, ranks) + blocks) / ranks;
  const uint64_t offset = pgas_gva_offset_of(gva, ranks, bsize) + cycles;

  const pgas_gva_t next = pgas_gva_from_triple(locality, offset, phase, ranks,
                                               bsize);

  // sanity check
  const uint32_t nextho = pgas_gva_heap_offset_of(next, ranks);
  DEBUG_IF (!heap_offset_inbounds(global_heap, nextho)) {
    dbg_error("computed out of bounds address\n");
  }

  return next;
}

pgas_gva_t pgas_gva_add(pgas_gva_t gva, int64_t bytes, uint32_t ranks) {
  return gva + bytes;
}

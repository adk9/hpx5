// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <inttypes.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/gpa.h>

/// Compute the phase (offset within block) of a global address.
static uint32_t _phase_of(hpx_addr_t gpa, uint32_t bsize) {
  uint32_t bsize_log2 = ceil_log2_32(bsize);
  uint64_t mask = (1ul << bsize_log2) - 1;
  uint64_t phase = mask & gpa;
  dbg_assert(phase < UINT32_MAX);
  return (uint32_t)phase;
}

/// Compute the block ID for a global address.
static uint64_t _block_of(hpx_addr_t addr, uint32_t bsize) {
  gpa_t       gpa = { .addr = addr };
  uint32_t rshift = ceil_log2_32(bsize);
  uint64_t  block = gpa.bits.offset >> rshift;
  return block;
}

static hpx_addr_t _triple_to_gpa(uint32_t rank, uint64_t bid, uint32_t phase,
                                 uint32_t bsize) {
  // make sure that the phase is in the expected range, locality will be checked
  DEBUG_IF (bsize && phase) {
    if (phase >= bsize) {
      dbg_error("phase %u must be less than %u\n", phase, bsize);
    }
  }

  DEBUG_IF (!bsize && phase) {
    dbg_error("cannot initialize a non-cyclic gpa with a phase of %u\n", phase);
  }

  // forward to pgas_offset_to_gpa(), by computing the offset by combining bid
  // and phase
  uint32_t shift = (bsize) ? ceil_log2_32(bsize) : 0;
  uint64_t offset = (bid << shift) + phase;
  return offset_to_gpa(rank, offset);
}


/// Forward declaration to be used in checking of address difference.
static hpx_addr_t _add_cyclic(hpx_addr_t, int64_t, uint32_t, bool);

/// Implementation of address difference to be called from the public interface.
/// The debug parameter is used to stop recursion in debugging checks.
static int64_t _sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize,
                           bool check) {
  // for a block cyclic computation, we look at the three components
  // separately, and combine them to get the overall offset
  const uint32_t plhs = _phase_of(lhs, bsize);
  const uint32_t prhs = _phase_of(rhs, bsize);
  const uint32_t llhs = gpa_to_rank(lhs);
  const uint32_t lrhs = gpa_to_rank(rhs);
  const uint64_t blhs = _block_of(lhs, bsize);
  const uint64_t brhs = _block_of(rhs, bsize);

  const int64_t dphase = plhs - (int64_t)prhs;
  const int32_t dlocality = llhs - lrhs;
  const int64_t dblock = blhs - brhs;

  // each difference in the phase is just one byte,
  // each difference in the locality is bsize bytes, and
  // each difference in the phase is entire cycle of bsize bytes
  const int64_t d = dblock * (int64_t) here->ranks * (int64_t) bsize +
                    dlocality * (int64_t) bsize +
                    dphase;
  if (!check) {
    return d;
  }

  // make sure we're not crazy
  dbg_assert_str(_add_cyclic(rhs, d, bsize, false) == lhs, "Address difference "
                 "%"PRIu64"-%"PRIu64 " computed incorrectly as %"PRId64"\n",
                 lhs, rhs, d);
  return d;
}


/// Implementation of address addition to be called from the public interface.
/// The debug parameter is used to stop recursion in debugging checks.
static hpx_addr_t _add_cyclic(hpx_addr_t gpa, int64_t n, uint32_t bsize,
                              bool check) {
  if (!bsize) {
    return gpa + n;
  }

  uint32_t phase, rank, cycles, block;
  int64_t  blocks;

  if (_phase_of(gpa, bsize) + n >= 0) {
    phase = (_phase_of(gpa, bsize) + n) % bsize;
    blocks = (_phase_of(gpa, bsize) + n) / bsize;
    rank = (gpa_to_rank(gpa) + blocks) % here->ranks;
    cycles = (gpa_to_rank(gpa) + blocks) / here->ranks;
  } else {
    phase = (bsize + (_phase_of(gpa, bsize) + n) % bsize) % bsize;
    blocks = (_phase_of(gpa, bsize) + n + 1) / bsize - 1;
    if (gpa_to_rank(gpa) + blocks >= 0) {
      rank = (gpa_to_rank(gpa) + blocks) % here->ranks;
      cycles = (gpa_to_rank(gpa) + blocks) / here->ranks;
    } else {
      rank = (here->ranks + (gpa_to_rank(gpa) + blocks) % 
              here->ranks) % here->ranks;
      cycles = (gpa_to_rank(gpa) + blocks + 1) / here->ranks - 1;
    }
  }
  block = _block_of(gpa, bsize) + cycles;

  const hpx_addr_t addr = _triple_to_gpa(rank, block, phase, bsize);
  if (!check) {
    return addr;
  }

  // sanity check
  const int64_t diff = _sub_cyclic(addr, gpa, bsize, false);
  dbg_assert_str(diff == n, "Address %"PRIu64"+%"PRId64" computed as %"PRIu64"."
                 " Expected %"PRId64".\n", gpa, n, addr, diff);
  return addr;
  (void)diff;
}

int64_t gpa_sub_cyclic(hpx_addr_t lhs, hpx_addr_t rhs, uint32_t bsize) {
  return _sub_cyclic(lhs, rhs, bsize, true);
}

hpx_addr_t gpa_add_cyclic(hpx_addr_t gpa, int64_t bytes, uint32_t bsize) {
  return _add_cyclic(gpa, bytes, bsize, true);
}

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

#include "gva.h"

int64_t pgas_gva_sub(pgas_gva_t lhs, pgas_gva_t rhs, uint32_t ranks,
                     uint32_t bsize)
{
  if (!bsize) {
    DEBUG_IF (pgas_gva_locality_of(lhs, ranks) !=
              pgas_gva_locality_of(rhs, ranks)) {
      dbg_error("local arithmetic failed between %u and %u\n",
                pgas_gva_locality_of(lhs, ranks), pgas_gva_locality_of(rhs,
                                                                       ranks));
    }
    return lhs - rhs;
  }
  else {
    const int32_t dphase = pgas_gva_phase_of(lhs, bsize) -
                           pgas_gva_phase_of(rhs, bsize);
    const int32_t dblocks = pgas_gva_locality_of(lhs, ranks) -
                            pgas_gva_locality_of(rhs, ranks);
    const int64_t dcycles = pgas_gva_offset_of(lhs, ranks, bsize) -
                            pgas_gva_offset_of(rhs, ranks, bsize);

    const int64_t d = dcycles * ranks * bsize + dblocks * bsize + dphase;
    DEBUG_IF (pgas_gva_add(lhs, d, ranks, bsize) != rhs) {
      dbg_error("difference between %lu and %lu computed incorrectly as %ld\n",
                lhs, rhs, d);
    }
    return d;
  }
}


/// Perform address arithmetic on a PGAS global address.
pgas_gva_t pgas_gva_add(pgas_gva_t gva, int64_t bytes, uint32_t ranks,
                        uint32_t bsize) {
  if (!bsize)
    return gva + bytes;

  const uint32_t phase = (pgas_gva_phase_of(gva, bsize) + bytes) % bsize;
  const uint32_t blocks = (pgas_gva_phase_of(gva, bsize) + bytes) / bsize;
  const uint32_t locality = (pgas_gva_locality_of(gva, ranks) + blocks) % ranks;
  const uint32_t cycles = (pgas_gva_locality_of(gva, ranks) + blocks) / ranks;
  const uint64_t offset = pgas_gva_offset_of(gva, ranks, bsize) + cycles;

  return pgas_gva_from(locality, offset, phase, ranks, bsize);
}


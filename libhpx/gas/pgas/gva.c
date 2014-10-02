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
#include "libhpx/locality.h"
#include "gva.h"

// GCC complains about int64_t bitfields when -pedantic is passed, so turn it
// off.
HPX_PUSH_IGNORE(-pedantic)

typedef struct {
  int64_t    phase : 16;
  int64_t   offset : 32;
  int64_t locality : 15;
  int64_t   cyclic : 1;
} _cyclic_gva_t;

typedef struct {
  int64_t   offset : 48;
  int64_t locality : 15;
  int64_t   cyclic : 1;
} _local_gva_t;

typedef union {
  int64_t atomic;
  _cyclic_gva_t cyclic;
  _local_gva_t local;
} _gva_t;

HPX_POP_IGNORE

static bool _cyclic(_gva_t gva) {
  return (gva.atomic < 0);
}

hpx_locality_t pgas_locality_of(hpx_addr_t gva) {
  const _gva_t pgva = { .atomic = gva.offset };
  return pgva.local.locality;
}

uint64_t pgas_offset_of(hpx_addr_t gva) {
  const _gva_t pgva = { .atomic = gva.offset };
  return (_cyclic(pgva)) ? pgva.cyclic.offset : pgva.local.offset;
}

uint16_t pgas_phase_of(hpx_addr_t gva) {
  const _gva_t pgva = { .atomic = gva.offset };
  return pgva.cyclic.phase;
}

static int64_t _cyclic_sub(_cyclic_gva_t lhs, _cyclic_gva_t rhs, uint16_t bsize)
{
  const int16_t bytes = lhs.phase - rhs.phase;
  const int16_t blocks = lhs.locality - rhs.locality;
  const int32_t cycles = lhs.offset - rhs.offset;

  return cycles * here->ranks * bsize + blocks * bsize + bytes;
}

static int64_t _local_sub(_local_gva_t lhs, _local_gva_t rhs) {
  DEBUG_IF (lhs.locality != rhs.locality) {
    dbg_error("local arithmetic failed between %u and %u\n", lhs.locality,
              rhs.locality);
  }
  return (lhs.offset - rhs.offset);
}

int64_t pgas_sub(hpx_addr_t lhs, hpx_addr_t rhs, uint16_t bsize) {
  const _gva_t l = { .atomic = lhs.offset };
  const _gva_t r = { .atomic = rhs.offset };

  DEBUG_IF ((_cyclic(l) || (!_cyclic(r)))) {
    dbg_error("can not compare local with cyclic addresses\n");
  }

  return unlikely(_cyclic(l)) ? _cyclic_sub(l.cyclic, r.cyclic, bsize)
                              : _local_sub(l.local, r.local);
}

/// Create a new hpx addr from a GVA.
static hpx_addr_t _to_addr(_gva_t gva, uint16_t bsize) {
  return hpx_addr_init(gva.atomic, 0, bsize);
}

/// Compute a new cyclic address from a gva when the displacement is not too
/// large.
///
/// 64-bit integer divides have almost the latency of cache misses, and can't be
/// pipelined. This version of the code uses 32-bit arithmetic which is less
/// than half the latency, when the displacement is small.
///
/// The cyclic add cycles first through the phase, then through the locality,
/// then through the offset, depending on the block size.
static hpx_addr_t _cyclic_add_near(_gva_t gva, int32_t bytes, uint16_t bsize,
                                   uint32_t ranks) {
  const int32_t blocks = bytes / bsize;
  gva.cyclic.phase += bytes % bsize;
  gva.cyclic.locality += blocks / ranks;
  gva.cyclic.offset += blocks % ranks;
  return _to_addr(gva, bsize);
}

/// Compute a new cyclic address from a gva when the displacement is large.
///
/// 64-bit integer divides have almost the latency of cache misses, and can't be
/// pipelined. This version should only be used for displacements larger than
/// 2^32.
///
/// The cyclic add cycles first through the phase, then through the locality,
/// then through the offset, depending on the block size.
static hpx_addr_t _cyclic_add_far(_gva_t gva, int64_t bytes, uint16_t bsize,
                                  uint32_t ranks) {
  const int64_t blocks = bytes / bsize;
  gva.cyclic.phase += bytes % bsize;
  gva.cyclic.locality += blocks / ranks;
  gva.cyclic.offset += blocks % ranks;
  return _to_addr(gva, bsize);
}

/// Compute an absolute value without branching.
///
/// From https://graphics.stanford.edu/~seander/bithacks.html#IntegerAbs.
static uint64_t _abs(int64_t v) {
  const int64_t mask = v >> (sizeof(mask) * 8 - 1);
  return (uint64_t)((v + mask) ^ mask);
}

/// Add a displacement to a cyclic address.
static HPX_NOINLINE hpx_addr_t _cyclic_add(_gva_t gva, int64_t bytes,
                                           uint16_t bsize)
{
  const uint32_t ranks = here->ranks;
  const bool near = _abs(bytes) < (uint64_t)UINT32_MAX;
  return (near) ? _cyclic_add_near(gva, (int32_t)bytes, bsize, ranks)
                : _cyclic_add_far(gva, bytes, bsize, ranks);
}

static hpx_addr_t _local_add(_gva_t gva, int64_t bytes) {
  gva.local.offset += bytes;
  return hpx_addr_init(gva.atomic, 0, 0);
}

/// Perform address arithmetic on a PGAS global address.
hpx_addr_t pgas_add(hpx_addr_t gva, int64_t bytes, uint16_t bsize) {
  const _gva_t pgva = { .atomic = gva.offset };
  return unlikely(_cyclic(pgva)) ? _cyclic_add(pgva, bytes, bsize)
                                 : _local_add(pgva, bytes);
}

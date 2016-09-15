// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_GAS_AGAS_BST_H
#define LIBHPX_GAS_AGAS_BST_H

#include "GlobalVirtualAddress.h"
#include "libhpx/parcel.h"
#include "hpx/hpx.h"
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include <cinttypes>
#include "rebalancer.h"

namespace libhpx {
namespace gas {
namespace agas {
class BlockStatisticsTable
{
 public:
  using GVA = GlobalVirtualAddress;

  BlockStatisticsTable(size_t);
  ~BlockStatisticsTable();

  /// Insert a block statistics record for a GVA.
  void insert(GVA gva, uint64_t* counts, uint64_t* sizes);

  /// Update a block statistics record for a GVA.
  void upsert(GVA gva, uint64_t* counts, uint64_t* sizes);

  /// Clear the block statistics table.
  void clear(void);

  // Constructs a sparse graph in the compressed sparse row (CSR)
  // format from the global BST, and serializes it into an output
  // buffer.
  std::vector<unsigned char> serializeToBuffer();

  // Constructs a sparse graph in the compressed sparse row (CSR)
  // format from the global BST, and serializes it into an output
  // parcel.
  hpx_parcel_t* serializeToParcel();  

 private:
  // Serialize into an allocated buffer.
  size_t serializeTo(unsigned char* buf);

  // Maximum bytes required for the serialized parcel.
  size_t serializeMaxBytes();

  // A block statistics record.
  struct Entry {
    uint64_t* counts;
    uint64_t* sizes;
    Entry() : counts(NULL), sizes(NULL) {
    }
    Entry(uint64_t* c, uint64_t* s)
        : counts(c), sizes(s) {
    }
  };

  using Hash = CityHasher<GlobalVirtualAddress>;
  using Map = cuckoohash_map<GlobalVirtualAddress, Entry, Hash>;

  const unsigned rank_;                         //!< cache the local rank
  Map map_;                                     //!< the hashtable
};
} // namespace agas
} // namespace gas
} // namespace libhpx

#endif // LIBHPX_GAS_AGAS_BST_H

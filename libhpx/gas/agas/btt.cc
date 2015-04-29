// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include <farmhash.h>
#include <libcuckoo/cuckoohash_map.hh>
#include "btt.h"

namespace {
  class Entry {
   public:
    int64_t key;
    int32_t rank;
    int32_t count;
  };

  class Hasher {
   public:
    size_t operator()(const hpx_addr_t addr) const {
      return util::Hash64(reinterpret_cast<const char*>(&addr), sizeof(addr));
    }
  };

  typedef cuckoohash_map<hpx_addr_t, Entry, Hasher> BlockHashMap;

  class BlockTranslationTable : BlockHashMap {
   public:
    BlockTranslationTable(size_t);
  };
}

void *btt_new(size_t size) {
  return new BlockTranslationTable(size);
}

void btt_delete(void *obj) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  delete btt;
}

BlockTranslationTable::BlockTranslationTable(size_t size) : BlockHashMap(size) {
}

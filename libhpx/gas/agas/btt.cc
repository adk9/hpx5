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

  typedef cuckoohash_map<hpx_addr_t, Entry, Hasher> Map;

  class BlockTranslationTable : Map {
   public:
    BlockTranslationTable(size_t);
    bool trypin(hpx_addr_t gva, void** lva);
    void unpin(hpx_addr_t gva);
    uint32_t getOwner(hpx_addr_t gva) const;
  };
}

BlockTranslationTable::BlockTranslationTable(size_t size) : Map(size) {
}

bool
BlockTranslationTable::trypin(hpx_addr_t gva, void** lva) {
  return false;
}

void
BlockTranslationTable::unpin(hpx_addr_t gva) {
}

uint32_t
BlockTranslationTable::getOwner(hpx_addr_t gva) const {
  return 0;
}

void *
btt_new(size_t size) {
  return new BlockTranslationTable(size);
}

void
btt_delete(void* obj) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  delete btt;
}

bool
btt_try_pin(void* obj, hpx_addr_t gva, void** lva) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  return btt->trypin(gva, lva);
}

void
btt_unpin(void* obj, hpx_addr_t gva) {
  BlockTranslationTable *btt = static_cast<BlockTranslationTable*>(obj);
  btt->unpin(gva);
}

uint32_t
btt_owner_of(const void* obj, hpx_addr_t gva) {
  const BlockTranslationTable *btt =
  static_cast<const BlockTranslationTable*>(obj);
  btt->getOwner(gva);
}

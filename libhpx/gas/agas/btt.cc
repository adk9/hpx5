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
#include "gva.h"

namespace {
  class Hasher {
   public:
    size_t operator()(const uint64_t key) const {
      return util::Hash64(reinterpret_cast<const char*>(&key), sizeof(key));
    }
  };

  struct Entry : public std::tuple<int32_t, int32_t, void*> {
    Entry() : std::tuple<int32_t, int32_t, void*>(0, 0, NULL) {
    }
    Entry(int32_t owner, void *lva) :
      std::tuple<int32_t, int32_t, void*>(0, owner, lva) {
    }
  };

  typedef cuckoohash_map<hpx_addr_t, Entry, Hasher> Map;

  class BTT : public Map {
   public:
    BTT(size_t);
    bool trypin(hpx_addr_t gva, void** lva);
    void unpin(hpx_addr_t gva);
    uint32_t getOwner(hpx_addr_t gva) const;
  };
}

BTT::BTT(size_t size) : Map(size) {
}

bool
BTT::trypin(hpx_addr_t gva, void** lva) {
  uint64_t key = gva_to_key(gva);
  return update_fn(key, [lva](Entry& entry) {
      std::get<1>(entry)++;
      if (lva) {
        *lva = std::get<2>(entry);
      }
    });
}

void
BTT::unpin(hpx_addr_t gva) {
  uint64_t key = gva_to_key(gva);
  bool found = update_fn(gva, [](Entry& entry) {
      std::get<1>(entry)--;
    });
  assert(found);
}

uint32_t
BTT::getOwner(hpx_addr_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return std::get<0>(entry);
  }
  else {
    return gva_home(gva);
  }
}

void *
btt_new(size_t size) {
  return new BTT(size);
}

void
btt_delete(void* obj) {
  BTT *btt = static_cast<BTT*>(obj);
  delete btt;
}

void
btt_insert(void *obj, hpx_addr_t gva, int32_t owner, void *lva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool inserted = btt->insert(key, Entry(owner, lva));
  assert(inserted);
  (void)inserted;
}

void
btt_remove(void *obj, hpx_addr_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool erased = btt->erase(key);
  assert(erased);
  (void)erased;
}

bool
btt_try_pin(void* obj, hpx_addr_t gva, void** lva) {
  BTT *btt = static_cast<BTT*>(obj);
  return btt->trypin(gva, lva);
}

void
btt_unpin(void* obj, hpx_addr_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  btt->unpin(gva);
}

uint32_t
btt_owner_of(const void* obj, hpx_addr_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getOwner(gva);
}

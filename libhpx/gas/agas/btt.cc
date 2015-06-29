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

#include <libcuckoo/cuckoohash_map.hh>
#include <libcuckoo/city_hasher.hh>
#include "btt.h"

namespace {
  struct Entry {
    int32_t count;
    int32_t owner;
    void *lva;
    size_t blocks;
    hpx_parcel_t *onunpin;
    Entry() : count(0), owner(0), lva(NULL), blocks(1), onunpin(NULL) {
    }
    Entry(int32_t o, void *l, size_t b, hpx_parcel_t *p) : count(0), owner(o), lva(l), blocks(b), onunpin(p)
    {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BTT : public Map {
   public:
    BTT(size_t);
    hpx_parcel_t *trydelete(gva_t gva, hpx_parcel_t *p);
    bool trypin(gva_t gva, void** lva);
    hpx_parcel_t *unpin(gva_t gva);
    void *lookup(gva_t gva) const;
    uint32_t getOwner(gva_t gva) const;
    size_t getBlocks(gva_t gva) const;
  };
}

BTT::BTT(size_t size) : Map(size) {
}

hpx_parcel_t *
BTT::trydelete(gva_t gva, hpx_parcel_t *p) {
  uint64_t key = gva_to_key(gva);
  bool ret = false;
  bool found = update_fn(key, [&](Entry& entry) {
      if (entry.count > 0) {
        assert(entry.onunpin == NULL);
        entry.onunpin = p;
      } else {
        ret = true;
      }
    });
  assert(found);
  return (ret ? p : NULL);
}

bool
BTT::trypin(gva_t gva, void** lva) {
  uint64_t key = gva_to_key(gva);
  bool ret = true;
  bool found = update_fn(key, [&](Entry& entry) {
      if (!lva) {
        return;
      }

      if (entry.onunpin != NULL) {
        ret = false;
        return;
      }

      assert(entry.count >= 0);
      entry.count++;
      *lva = (char*)(entry.lva) + gva_to_block_offset(gva);
    });
  return found && ret;
}

hpx_parcel_t *
BTT::unpin(gva_t gva) {
  uint64_t key = gva_to_key(gva);
  bool ret = false;
  hpx_parcel_t *p;
  bool found = update_fn(key, [&](Entry& entry) {
      assert(entry.count > 0);
      entry.count--;
      if (entry.count == 0 && entry.onunpin) {
        p = entry.onunpin;
        entry.onunpin = NULL;
        ret = true;
      }
    });
  assert(found);
  return (ret ? p : NULL);
}

void *
BTT::lookup(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.lva;
  }
  else {
    return NULL;
  }
}

uint32_t
BTT::getOwner(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.owner;
  }
  else {
    return gva.bits.home;
  }
}

size_t
BTT::getBlocks(gva_t gva) const {
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = find(key, entry);
  if (found) {
    return entry.blocks;
  }
  else {
    return 0;
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
btt_insert(void *obj, gva_t gva, int32_t owner, void *lva, size_t blocks) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool inserted = btt->insert(key, Entry(owner, lva, blocks, NULL));
  assert(inserted);
  (void)inserted;
}

void
btt_remove(void *obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool erased = btt->erase(key);
  assert(erased);
  (void)erased;
}

void
btt_try_delete(void *obj, gva_t gva, hpx_parcel_t *p) {
  BTT *btt = static_cast<BTT*>(obj);
  p = btt->trydelete(gva, p);
  if (p) {
    hpx_parcel_send_sync(p);
  }
}

bool
btt_try_pin(void* obj, gva_t gva, void** lva) {
  BTT *btt = static_cast<BTT*>(obj);
  return btt->trypin(gva, lva);
}

void
btt_unpin(void* obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  hpx_parcel_t *p = btt->unpin(gva);
  if (p) {
    hpx_parcel_send_sync(p);
  }
}

void *
btt_lookup(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->lookup(gva);
}

uint32_t
btt_owner_of(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getOwner(gva);
}

size_t
btt_get_blocks(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  return btt->getBlocks(gva);
}

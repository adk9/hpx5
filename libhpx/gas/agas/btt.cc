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

#include <libhpx/action.h>
#include <inttypes.h>
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include "btt.h"

namespace {
  struct Entry {
    int32_t count;
    uint32_t owner;
    void *lva;
    size_t blocks;
    hpx_parcel_t *onunpin;
    uint32_t attr;
    Entry() : count(0), owner(0), lva(NULL), blocks(1), onunpin(NULL), attr(0) {
    }
    Entry(int32_t o, void *l, size_t b, uint32_t a) : count(0), owner(o), lva(l), blocks(b), onunpin(NULL), attr(a) {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BTT : public Map {
   public:
    BTT(size_t);
    bool tryPin(gva_t gva, void** lva);
    hpx_parcel_t *unpin(gva_t gva);
  };
}

BTT::BTT(size_t size) : Map(size) {
}

bool
BTT::tryPin(gva_t gva, void** lva) {
  uint64_t key = gva_to_key(gva);
  bool ret = false;
  bool found = update_fn(key, [&](Entry& entry) {
      // If we do not own the block or if there is a pending delete on
      // this block, the try-pin operation fails.
      if (entry.owner != here->rank) {
        return;
      }

      assert(entry.count >= 0);
      entry.count++;
      ret = true;

      if (lva) {
        // printf("%lu %d ++\n", key, entry.count);
        *lva = (char*)(entry.lva) + gva_to_block_offset(gva);
      }
    });
  return found && ret;
}

hpx_parcel_t *
BTT::unpin(gva_t gva) {
  uint64_t key = gva_to_key(gva);
  hpx_parcel_t *p = NULL;
  bool found = update_fn(key, [&](Entry& entry) {
      assert(entry.owner == here->rank);
      assert(entry.count > 0);
      entry.count--;
      // printf("%lu %d --\n", key, entry.count);
      if (!entry.count && entry.onunpin) {
        p = entry.onunpin;
        entry.onunpin = NULL;
      }
    });
  assert(found);
  return p;
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
btt_insert(void *obj, gva_t gva, uint32_t owner, void *lva, size_t blocks,
           uint32_t attr) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool inserted = btt->insert(key, Entry(owner, lva, blocks, attr));
  assert(inserted);
  (void)inserted;
}

void
btt_upsert(void *obj, gva_t gva, uint32_t owner, void *lva, size_t blocks,
           uint32_t attr) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  auto updatefn = [&](Entry& entry) {
    entry = Entry(owner, lva, blocks, attr);
  };
  btt->upsert(key, updatefn, Entry(owner, lva, blocks, attr));
}

void
btt_remove(void *obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  uint64_t key = gva_to_key(gva);
  bool erased = btt->erase(key);
  assert(erased);
  (void)erased;
}

bool
btt_try_pin(void* obj, gva_t gva, void** lva) {
  BTT *btt = static_cast<BTT*>(obj);
  return btt->tryPin(gva, lva);
}

void
btt_unpin(void* obj, gva_t gva) {
  BTT *btt = static_cast<BTT*>(obj);
  hpx_parcel_t *p = btt->unpin(gva);
  if (p) {
    scheduler_spawn(p);
  }
}

void *
btt_lookup(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  if (found) {
    return entry.lva;
  }
  else {
    return NULL;
  }
}

bool
btt_get_owner(const void* obj, gva_t gva, uint32_t *owner) {
  const BTT *btt = static_cast<const BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  if (owner) {
    *owner = found ? entry.owner : gva.bits.home;
  }
  return found;
}

bool
btt_get_attr(const void* obj, gva_t gva, uint32_t *attr) {
  const BTT *btt = static_cast<const BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  *attr = found ? entry.attr : HPX_GAS_ATTR_NONE;
  return found;
}

void
btt_set_attr(void* obj, gva_t gva, uint32_t attr) {
  BTT *btt = static_cast<BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->update_fn(key, [&](Entry& entry) {
      entry.attr |= attr;
    });
  assert(found);
}

size_t
btt_get_blocks(const void* obj, gva_t gva) {
  const BTT *btt = static_cast<const BTT*>(obj);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  if (found) {
    return entry.blocks;
  }
  return 0;
}

int
btt_get_all(const void *o, gva_t gva, void **lva, size_t *blocks,
            int32_t *cnt, uint32_t *owner, uint32_t *attr) {
  const BTT *btt = static_cast<const BTT*>(o);
  Entry entry;
  uint64_t key = gva_to_key(gva);
  bool found = btt->find(key, entry);
  if (found) {
    if (lva) {
      *lva = entry.lva;
    }
    if (blocks) {
      *blocks = entry.blocks;
    }
    if (cnt) {
      *cnt = entry.count;
    }
    if (owner) {
      *owner = entry.owner;
    }
    if (attr) {
      *attr = entry.attr;
    }
  }
  return found;
}

typedef struct {
  BTT        *btt;
  gva_t       gva;
  uint32_t     to;
  void      **lva;
  uint32_t  *attr;
  int32_t  *count;
} _btt_onunpin_env_t;

static void _btt_onunpin_continuation(hpx_parcel_t *p, void *e) {
  _btt_onunpin_env_t *env = static_cast<_btt_onunpin_env_t*>(e);
  BTT       *btt = env->btt;
  gva_t      gva = env->gva;
  int         to = env->to;
  void     **lva = env->lva;
  uint32_t *attr = env->attr;
  int32_t *count = env->count;

  uint64_t key = gva_to_key(gva);
  bool found = btt->update_fn(key, [&](Entry& entry) {
      if (lva) {
        *lva = entry.lva;
      }

      if (attr) {
        *attr = entry.attr;
      }

      *count = entry.count;
      if (!*count) {
        entry.owner = to;
      } else {
        entry.onunpin = p;
      }
    });
  assert(found);
  if (!*count) {
    scheduler_spawn(p);
  }
}

void btt_remove_when_count_zero(void *obj, gva_t gva, void **lva) {
  BTT *btt = static_cast<BTT*>(obj);

  int32_t count = 0;
  _btt_onunpin_env_t env = {
    .btt = btt,
    .gva = gva,
    .to  = here->rank,
    .lva = lva,
    .attr = NULL,
    .count = &count
  };

  do {
    scheduler_suspend(_btt_onunpin_continuation, &env, 0);
  } while (count != 0);
  btt_remove(obj, gva);
}

void btt_try_move(void *obj, gva_t gva, uint32_t to, void **lva,
                  uint32_t *attr) {
  BTT *btt = static_cast<BTT*>(obj);

  int32_t count = 0;
  _btt_onunpin_env_t env = {
    .btt = btt,
    .gva = gva,
    .to = to,
    .lva = lva,
    .attr = attr,
    .count = &count
  };

  do {
    scheduler_suspend(_btt_onunpin_continuation, &env, 0);
  } while (count != 0);
}

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
#include "chunk_table.h"

namespace {
  class Hasher {
   public:
    size_t operator()(const void* key) const {
      return util::Hash64(reinterpret_cast<const char*>(&key), sizeof(key));
    }
  };

  typedef cuckoohash_map<const void*, uint64_t, Hasher> ChunkTable;
}

void *
chunk_table_new(size_t size) {
  return new ChunkTable(size);
}

void
chunk_table_delete(void *obj) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  delete table;
}

uint64_t
chunk_table_lookup(void *obj, void *chunk) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  uint64_t base;
  bool found = table->find(chunk, base);
  assert(found);
  return base;
}

void
chunk_table_insert(void *obj, void *chunk, uint64_t base) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  bool inserted = table->insert(chunk, base);
  assert(inserted);
  (void)inserted;
}

void
chunk_table_remove(void *obj, void *chunk) {
  ChunkTable *table = static_cast<ChunkTable*>(obj);
  bool erased = table->erase(chunk);
  assert(erased);
  (void)erased;
}


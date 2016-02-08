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

#include <cassert>
#include <libhpx/libhpx.h>
#include <libhpx/parcel.h>
#include <libhpx/scheduler.h>
#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include "rebalancing.h"

namespace {
  struct Entry {
    uint64_t *counts;
    uint64_t *sizes;
    Entry() : counts(NULL), sizes(NULL) {
    }
    Entry(uint64_t *c, uint64_t *s) : counts(c), sizes(s) {
    }
  };

  typedef cuckoohash_map<uint64_t, Entry, CityHasher<uint64_t> > Map;

  class BST : public Map {
   public:
    BST(size_t);
  };
}

BST::BST(size_t size) : Map(size) {
}

void *
bst_new(size_t size) {
  return new BST(size);
}

void
bst_delete(void* obj) {
  BST *bst = static_cast<BST*>(obj);
  delete bst;
}

size_t
bst_serialize_to_parcel(void* obj, hpx_parcel_t **parcel) {
  BST *bst = static_cast<BST*>(obj);

  int nvtxs = bst->size();
  size_t buf_size = (4*nvtxs*sizeof(uint64_t)) + (3*sizeof(uint64_t))
      + (2*nvtxs*here->ranks*sizeof(uint64_t));
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, buf_size);
  assert(p);
  uint64_t *base = static_cast<uint64_t*>(hpx_parcel_get_data(p));

  // store the vertex count first
  base[0] = nvtxs;

  // next, store the vtxs, vwgt and vsizes array each (nvtxs *
  // sizeof(uint64_t)) bytes long
  uint64_t *vtxs = &base[1];
  uint64_t *vwgt = &base[nvtxs+1];
  uint64_t *vsizes = &base[2*nvtxs+1];

  // xadj is (nvtxs+1 * sizeof(uint64_t)) bytes long
  uint64_t *xadj = &base[3*nvtxs+1];

  uint64_t *nedges = &base[4*nvtxs+2];
  *nedges = 0;
  // the next two arrays are (nvtxs * here->ranks * sizeof(uint64_t))
  // bytes long
  uint64_t *adjncy = nedges++;
  uint64_t *adjwgt = (uint64_t*)calloc(nvtxs*here->ranks, sizeof(uint64_t));
  assert(adjncy && adjwgt);

  int i = 0;
  int nbrs = 0;
  {
    auto lt = bst->lock_table();
    for (const auto& item : lt) {
      Entry entry = item.second;
      xadj[i] = nbrs;
      uint64_t total_vwgt = 0;
      uint64_t total_vsize = 0;
      for (unsigned k = 0; k < here->ranks; ++k) {
        if (k == here->rank) {
          continue;
        }

        if (entry.counts[k] != 0) {
          assert(entry.sizes[k] != 0);

          adjncy[nbrs] = k;
          adjwgt[nbrs] = entry.counts[k] * entry.sizes[k];
          total_vwgt += entry.counts[k];
          total_vsize += entry.sizes[k];
          nbrs++;
        }
        *nedges += nbrs;
      }

      vtxs[i] = item.first;
      vwgt[i] = total_vwgt;
      vsizes[i] = total_vsize;
      i++;
      free(entry.counts);
      free(entry.sizes);
    }
  }

  p->size = (4*nvtxs*sizeof(uint64_t)) + (3*sizeof(uint64_t))
          + (2*(*nedges)*sizeof(uint64_t));
  memcpy(adjncy+(*nedges), adjwgt, (*nedges)*sizeof(uint64_t));
  free(adjwgt);
  *parcel = p;
  return p->size;
}

void
bst_upsert(void *obj, uint64_t block, uint64_t *counts, uint64_t *sizes) {
  BST *bst = static_cast<BST*>(obj);
  auto updatefn = [&](Entry& entry) {
    for (unsigned i = 0; i < here->ranks; ++i) {
      entry.counts[i] += counts[i];
      entry.sizes[i]  += sizes[i];
    }
    free(counts);
    free(sizes);
  };
  bst->upsert(block, updatefn, Entry(counts, sizes));
}

void
bst_remove(void *obj, uint64_t block) {
  BST *bst = static_cast<BST*>(obj);
  bool erased = bst->erase(block);
  assert(erased);
  (void)erased;
}

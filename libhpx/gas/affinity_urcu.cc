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

#include <city_hasher.hh>
#include <mutex>
#include <stack>
#include <urcu-qsbr.h>
#include <urcu/rculfhash.h>
#include <hpx/hpx.h>
#include "affinity.h"

namespace {
/// Map to the correct CityHash handler.
/// @{
template <size_t N = sizeof(unsigned long)>
unsigned long Hash(hpx_addr_t gva);

template <>
unsigned long Hash<8>(hpx_addr_t gva) {
  return CityHash64(reinterpret_cast<const char* const>(&gva), sizeof(gva));
}

template <>
unsigned long Hash<4>(hpx_addr_t gva) {
  return CityHash32(reinterpret_cast<const char* const>(&gva), sizeof(gva));
}
/// @}

/// Simple RCU read lock for use in the lock guard RAII paradigm.
class RCULock {
 public:
  static void lock() {
    rcu_read_lock();
  }

  static void unlock() {
    rcu_read_unlock();
  }
};

/// A hash table node, inherits from the RCU hash table node type.
struct Node : public cds_lfht_node {
  Node(int k, int v) : key(k), value(v) {
    cds_lfht_node_init(this);
  }

  ~Node() {
  }

  static int Match(struct cds_lfht_node *ht_node, const void *key) {
    Node *node = static_cast<Node*>(ht_node);
    return (node->key == *static_cast<const int*>(key));
  }

  int key;
  int value;
};

class AffinityMap  {
 public:
  AffinityMap() : ht(cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL)) {
  }

  ~AffinityMap() {
  }

  void set(hpx_addr_t gva, int worker) {
    if (Node *n = insert(Hash(gva), new Node(gva, worker))) {
      synchronize_rcu();
      delete n;
    }
  }

  int get(hpx_addr_t gva) const {
    return lookup(Hash(gva), gva);
  }

  void clear(hpx_addr_t gva) {
    if (Node *n = remove(Hash(gva), gva)) {
      synchronize_rcu();
      delete n;
    }
  }

 private:
  Node *insert(unsigned long hash, Node *n) {
    std::lock_guard<RCULock> guard();
    return static_cast<Node*>(cds_lfht_add_replace(ht, hash, Node::Match, &n->key, n));
  }

  int lookup(unsigned long hash, hpx_addr_t gva) const {
    std::lock_guard<RCULock> guard();
    struct cds_lfht_iter it;
    cds_lfht_lookup(ht, hash, Node::Match, &gva, &it);
    Node *n = static_cast<Node*>(cds_lfht_iter_get_node(&it));
    return (n) ? n->value : -1;
  }

  Node *remove(int hash, hpx_addr_t gva) {
    std::lock_guard<RCULock> guard();
    struct cds_lfht_iter it;
    cds_lfht_lookup(ht, hash, Node::Match, NULL, &it);
    if (Node *n = static_cast<Node*>(cds_lfht_iter_get_node(&it))) {
      if (cds_lfht_del(ht, n) == 0) {
        return n;
      }
    }
    return NULL;
  }

  struct cds_lfht * const ht;
};

AffinityMap *map = NULL;
}

void affinity_init(void *UNUSED) {
  map = new AffinityMap();
}

void affinity_fini(void *UNUSED) {
  delete map;
}

void affinity_set(void *UNUSED, hpx_addr_t gva, int worker) {
  map->set(gva, worker);
}

void affinity_clear(void *UNUSED, hpx_addr_t gva) {
  map->clear(gva);
}

int affinity_get(const void *UNUSED, hpx_addr_t gva) {
  return map->get(gva);
}

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
#include <vector>
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
template <typename Key, typename Value>
struct Node : public cds_lfht_node {
  Node(Key k, Value v) : key(k), value(v) {
    cds_lfht_node_init(this);
  }

  ~Node() {
  }

  static int Match(struct cds_lfht_node *node, const void *key) {
    const Node<Key, Value>& n = *static_cast<const Node<Key, Value>*>(node);
    const Key& k = *static_cast<const Key*>(key);
    return (n.key == k);
  }

  Key key;
  Value value;
};

template <typename Key, typename Value>
class AffinityMap {
  typedef Node<Key, Value> HashNode;
  typedef unsigned long hash_t;
 public:
  AffinityMap() : ht(cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL)) {
  }

  ~AffinityMap() {
    std::vector<HashNode*> nodes;
    removeAll(nodes);
    synchronize_rcu();

    for (int i = 0, e = nodes.size(); i < e; ++i) {
      delete nodes[i];
    }

    if (cds_lfht_destroy(ht, NULL) != 0) {
      throw std::runtime_error("Failed to destroy the urcu hash table\n");
    }
  }

  void set(Key k, Value v) {
    if (HashNode *n = insert(Hash(k), new HashNode(k, v))) {
      synchronize_rcu();
      delete n;
    }
  }

  int get(Key k) const {
    return lookup(Hash(k), k);
  }

  void clear(Key k) {
    if (HashNode *n = remove(Hash(k), k)) {
      synchronize_rcu();
      delete n;
    }
  }

 private:
  HashNode *insert(hash_t hash, HashNode *node) {
    std::lock_guard<RCULock> guard();
    Key key = node->key;
    void *n = cds_lfht_add_replace(ht, hash, HashNode::Match, &key, node);
    return static_cast<HashNode*>(n);
  }

  int lookup(hash_t hash, Key key) const {
    std::lock_guard<RCULock> guard();
    struct cds_lfht_iter it;
    cds_lfht_lookup(ht, hash, HashNode::Match, &key, &it);
    HashNode *n = static_cast<HashNode*>(cds_lfht_iter_get_node(&it));
    return (n) ? n->value : -1;
  }

  HashNode *remove(hash_t hash, Key key) {
    std::lock_guard<RCULock> guard();
    struct cds_lfht_iter it;
    cds_lfht_lookup(ht, hash, HashNode::Match, &key, &it);
    if (HashNode *n = static_cast<HashNode*>(cds_lfht_iter_get_node(&it))) {
      if (cds_lfht_del(ht, n) == 0) {
        return n;
      }
    }
    return NULL;
  }

  void removeAll(std::vector<HashNode*>& nodes) {
    struct cds_lfht_iter it;
    struct cds_lfht_node *node;
    std::lock_guard<RCULock> guard();
    cds_lfht_for_each(ht, &it, node) {
      if (cds_lfht_del(ht, node) != 0) {
        throw std::runtime_error("Failed to delete node\n");
      }
      nodes.push_back(static_cast<HashNode*>(node));
    }
  }

  struct cds_lfht * const ht;
};

AffinityMap<hpx_addr_t, int> *map = NULL;
}

void affinity_init(void *UNUSED) {
  map = new AffinityMap<hpx_addr_t, int>();
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

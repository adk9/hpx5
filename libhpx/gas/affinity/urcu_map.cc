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
#include <urcu-qsbr.h>
#include <urcu/rculfhash.h>
#include <vector>
#include "urcu_map.h"

using libhpx::gas::Affinity;
using libhpx::gas::URCUMap;

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
struct RCULock {
  static void lock() {
    rcu_read_lock();
  }

  static void unlock() {
    rcu_read_unlock();
  }
};
}

/// A hash table node, inherits from the RCU hash table node type.
struct URCUMap::Node : public cds_lfht_node {
  Node(hpx_addr_t k, int v) : key(k), value(v) {
    cds_lfht_node_init(this);
  }

  ~Node() {
  }

  static int Match(struct cds_lfht_node *node, const void *key) {
    const Node& n = *static_cast<const Node*>(node);
    const hpx_addr_t& k = *static_cast<const hpx_addr_t*>(key);
    return (n.key == k);
  }

  hpx_addr_t key;
  hpx_addr_t value;
};

URCUMap::URCUMap()
  : ht(cds_lfht_new(1, 1, 0, CDS_LFHT_AUTO_RESIZE, NULL))
{
}

URCUMap::~URCUMap()
{
  std::vector<Node*> nodes;
  removeAll(nodes);
  synchronize_rcu();

  for (int i = 0, e = nodes.size(); i < e; ++i) {
    delete nodes[i];
  }

  if (cds_lfht_destroy(ht, NULL) != 0) {
    throw std::runtime_error("Failed to destroy the urcu hash table\n");
  }
}

void
URCUMap::set(hpx_addr_t k, int v)
{
  if (Node *n = insert(Hash(k), new Node(k, v))) {
    synchronize_rcu();
    delete n;
  }
}

int
URCUMap::get(hpx_addr_t k) const
{
  return lookup(Hash(k), k);
}

void
URCUMap::clear(hpx_addr_t k)
{
  if (Node *n = remove(Hash(k), k)) {
    synchronize_rcu();
    delete n;
  }
}

URCUMap::Node*
URCUMap::insert(hash_t hash, URCUMap::Node *node)
{
  std::lock_guard<RCULock> guard();
  hpx_addr_t key = node->key;
  void *n = cds_lfht_add_replace(ht, hash, Node::Match, &key, node);
  return static_cast<Node*>(n);
}

URCUMap::Node*
URCUMap::remove(hash_t hash, hpx_addr_t key)
{
  std::lock_guard<RCULock> guard();
  struct cds_lfht_iter it;
  cds_lfht_lookup(ht, hash, Node::Match, &key, &it);
  if (Node *n = static_cast<Node*>(cds_lfht_iter_get_node(&it))) {
    if (cds_lfht_del(ht, n) == 0) {
      return n;
    }
  }
  return NULL;
}

void
URCUMap::removeAll(std::vector<URCUMap::Node*>& nodes)
{
  struct cds_lfht_iter it;
  struct cds_lfht_node *node;
  std::lock_guard<RCULock> guard();
  cds_lfht_for_each(ht, &it, node) {
    if (cds_lfht_del(ht, node) != 0) {
      throw std::runtime_error("Failed to delete node\n");
    }
    nodes.push_back(static_cast<Node*>(node));
  }
}

int
URCUMap::lookup(hash_t hash, hpx_addr_t key) const
{
  std::lock_guard<RCULock> guard();
  struct cds_lfht_iter it;
  cds_lfht_lookup(ht, hash, Node::Match, &key, &it);
  Node *n = static_cast<Node*>(cds_lfht_iter_get_node(&it));
  return (n) ? n->value : -1;
}

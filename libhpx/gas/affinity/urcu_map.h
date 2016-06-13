// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_GAS_AFFINITY_URCU_MAP_H
#define LIBHPX_GAS_AFFINITY_URCU_MAP_H

#include <vector>
#include <libhpx/gas.h>

extern "C" struct cds_lfht;

namespace libhpx {
namespace gas {
class URCUMap : public Affinity  {
 public:
  URCUMap();
  virtual ~URCUMap();

  void set(hpx_addr_t gva, int worker);
  void clear(hpx_addr_t gva);
  int get(hpx_addr_t gva) const;

 private:
  typedef unsigned long hash_t;
  struct Node;

  Node* insert(hash_t hash, Node* node);
  Node* remove(hash_t hash, hpx_addr_t key);
  void removeAll(std::vector<Node*>& nodes);
  int lookup(hash_t hash, hpx_addr_t key) const;

  struct cds_lfht * const ht;
};
}
}

#endif // LIBHPX_GAS_AFFINITY_URCU_MAP_H

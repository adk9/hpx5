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

#ifndef LIBHPX_GAS_AFFINITY_CUCKOO_HASH_H
#define LIBHPX_GAS_AFFINITY_CUCKOO_HASH_H

#include <cuckoohash_map.hh>
#include <city_hasher.hh>
#include <libhpx/gas.h>

namespace libhpx {
namespace gas {
class CuckooHash : public Affinity  {
 public:
  CuckooHash();
  virtual ~CuckooHash();

  void set(hpx_addr_t gva, int worker);
  void clear(hpx_addr_t gva);
  int get(hpx_addr_t gva) const;

 private:
  cuckoohash_map<hpx_addr_t, int, CityHasher<hpx_addr_t> > map_;
};
}
}

#endif // LIBHPX_GAS_AFFINITY_CUCKOO_HASH_H

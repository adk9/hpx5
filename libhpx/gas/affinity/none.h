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

#ifndef LIBHPX_GAS_AFFINITY_NONE_H
#define LIBHPX_GAS_AFFINITY_NONE_H

#include <libhpx/gas.h>

namespace libhpx {
namespace gas {
class None : public Affinity {
 public:
  None();
  virtual ~None();

  void set(hpx_addr_t gva, int worker);
  void clear(hpx_addr_t gva);
  int get(hpx_addr_t gva) const;
};
}
}

#endif // LIBHPX_GAS_AFFINITY_NONE_H

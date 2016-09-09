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

#ifndef LIBHPX_GAS_AGAS_GLOBAL_VIRTUAL_ADDRESS_H
#define LIBHPX_GAS_AGAS_GLOBAL_VIRTUAL_ADDRESS_H

#include "libhpx/gpa.h"
#include "libhpx/util/math.h"
#include "hpx/hpx.h"
#include <cstdint>

namespace libhpx {
namespace gas {
namespace agas {
/// Set up some limitations for the AGAS implementation for now.
constexpr int GVA_RANK_BITS = 16;
constexpr int GVA_SIZE_BITS = 5;
constexpr int GVA_OFFSET_BITS = 42;

union GlobalVirtualAddress {
 private:
  hpx_addr_t addr_;
 public:
  struct {
    uint64_t offset : GVA_OFFSET_BITS;
    uint64_t   size : GVA_SIZE_BITS;
    uint64_t cyclic : 1;
    uint64_t   home : GVA_RANK_BITS;
  };

  GlobalVirtualAddress() : addr_(HPX_NULL) {
  }

  GlobalVirtualAddress(hpx_addr_t addr) : addr_(addr) {
  }

  GlobalVirtualAddress(uint64_t o, int s, int c, uint16_t h)
      : offset(o), size(s), cyclic(c), home(h) {
  }

  GlobalVirtualAddress add(hpx_gas_ptrdiff_t bytes, size_t bsize) const {
    assert(size == util::ceil_log2(bsize));
    if (cyclic) {
      return addCyclic(bytes, bsize);
    }
    else {
      return addLocal(bytes, bsize);
    }
  }

  hpx_gas_ptrdiff_t sub(GlobalVirtualAddress rhs, size_t bsize) const {
    assert(size == rhs.size);
    assert(size == util::ceil_log2(bsize));
    assert(!cyclic || rhs.cyclic);
    assert(!rhs.cyclic || cyclic);

    if (cyclic) {
      return gpa_sub_cyclic(addr_, rhs.addr_, bsize);
    }
    else {
      assert(home == rhs.home);
      return subLocal(rhs, bsize);
    }
  }

  operator hpx_addr_t() const {
    return addr_;
  }

  uint64_t toKey() const {
    uint64_t mask = ~((uint64_t(1) << size) - 1);
    return addr_ & mask;
  }

  uint64_t toBlockOffset() const {
    uint64_t mask = ((uint64_t(1) << size) - 1);
    return addr_ & mask;
  }

  hpx_addr_t getAddr() const {
    return addr_;
  }

  size_t getBlockSize() const {
    return uint64_t(1) << size;
  }

  bool operator==(const GlobalVirtualAddress& rhs) const {
    return addr_ == rhs.addr_;
  }

 private:
  GlobalVirtualAddress addCyclic(hpx_gas_ptrdiff_t bytes, size_t bsize) const {
    GlobalVirtualAddress lhs(gpa_add_cyclic(addr_, bytes, bsize));
    lhs.size = size;
    lhs.cyclic = 1;
    return lhs;
  }

  GlobalVirtualAddress addLocal(int64_t n, size_t bsize) const {
    int64_t blocks = n / bsize;
    int64_t bytes = n % bsize;
    uint64_t block_size = (uint64_t(1) << size);
    uint64_t addr = addr_ + blocks * block_size + bytes;
    dbg_assert((addr & (block_size - 1)) < bsize);
    return GlobalVirtualAddress(addr);
  }

  hpx_gas_ptrdiff_t subLocal(GlobalVirtualAddress rhs, size_t bsize) const {
    uint64_t mask = (uint64_t(1) << size) - 1;
    uint64_t plhs = offset & mask;
    uint64_t prhs = rhs.offset & mask;
    uint64_t blhs = offset >> size;
    uint64_t brhs = rhs.offset >> size;
    return (plhs - prhs) + (blhs - brhs) * bsize;
  }
};

static_assert(sizeof(GlobalVirtualAddress) == sizeof(hpx_addr_t),
              "GVA structure packing failed\n");

} // namespace agas
} // namespace gas
} // namespace libhpx

#endif // LIBHPX_GAS_AGAS_GLOBAL_VIRTUAL_ADDRESS_H

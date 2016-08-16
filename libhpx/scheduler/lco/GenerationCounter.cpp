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
#include "config.h"
#endif

/// @file libhpx/scheduler/sema.c
/// @brief Implements the semaphore LCO.

#include "LCO.h"
#include "Condition.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/memory.h"
#include <mutex>
#include <cstring>

namespace {
using libhpx::scheduler::Condition;
using libhpx::scheduler::LCO;

struct GenerationCount final : public LCO {
 public:
  GenerationCount(unsigned ninplace);
  ~GenerationCount();

  int set(size_t size, const void *value);
  void error(hpx_status_t code);
  hpx_status_t get(size_t size, void *value, int reset);
  hpx_status_t wait(int reset);
  hpx_status_t attach(hpx_parcel_t *p);
  void reset();
  size_t size(size_t size) const {
    return sizeof(GenerationCount) + ninplace_ * sizeof(Condition);
  }

  hpx_status_t waitForGeneration(unsigned long gen);

  static int NewHandler(void* buffer, unsigned ninplace);
  static int WaitForGenerationHandler(GenerationCount& lco, unsigned long gen);

 private:
  Condition            oflow_;
  volatile unsigned long gen_;
  const unsigned    ninplace_;
  Condition          inplace_[];
};

LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, New, GenerationCount::NewHandler,
              HPX_POINTER, HPX_UINT);
LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED, WaitForGeneration,
              GenerationCount::WaitForGenerationHandler, HPX_POINTER,
              HPX_ULONG);
}

GenerationCount::GenerationCount(unsigned ninplace)
    : LCO(LCO_GENCOUNT),
      gen_(),
      ninplace_(ninplace),
      inplace_()
{
  for (unsigned i = 0, e = ninplace; i < e; ++i) {
    new(inplace_ + i) Condition();
  }
}

GenerationCount::~GenerationCount()
{
  lock();
  for (unsigned i = 0, e = ninplace_; i < e; ++i) {
    inplace_[i].~Condition();
  }
}

void
GenerationCount::error(hpx_status_t code)
{
  std::lock_guard<LCO> _(*this);
  for (unsigned i = 0, e = ninplace_; i < e; ++i) {
    inplace_[i].signalError(code);
  }
  oflow_.signalError(code);
}

void
GenerationCount::reset()
{
  std::lock_guard<LCO> _(*this);
  for (unsigned i = 0, e = ninplace_; i < e; ++i) {
    inplace_[i].reset();
  }
  oflow_.reset();
}

int
GenerationCount::set(size_t size, const void *from)
{
  std::lock_guard<LCO> _(*this);
  unsigned long i = ++gen_;
  oflow_.signalAll();
  if (ninplace_) {
    inplace_[i % ninplace_].signalAll();
  }
  return 1;
}

hpx_status_t
GenerationCount::get(size_t size, void *out, int reset)
{
  // @note No lock here... just a single volatile read is good enough
  if (size && out) {
    unsigned long i = gen_;
    memcpy(out, &i, size);
  }
  return HPX_SUCCESS;
}

hpx_status_t
GenerationCount::wait(int reset)
{
  std::lock_guard<LCO> _(*this);
  return oflow_.wait(this);
}

hpx_status_t
GenerationCount::attach(hpx_parcel_t *p)
{
  std::lock_guard<LCO> _(*this);
  if (auto status = oflow_.getError()) {
    return status;
  }
  return oflow_.push(p);
}


hpx_status_t
GenerationCount::waitForGeneration(unsigned long i)
{
  std::lock_guard<LCO> _(*this);
  while (gen_ < i) {
    bool inplace = i < (gen_ + ninplace_);
    Condition& cond = (inplace) ? inplace_[i % ninplace_] : oflow_;
    if (auto status = cond.wait(this)) {
      return status;
    }
  }
  return oflow_.getError();
}

int
GenerationCount::NewHandler(void* buffer, unsigned ninplace)
{
  if (auto lco = new(buffer) GenerationCount(ninplace)) {
    return HPX_THREAD_CONTINUE(lco);
  }
  dbg_error("Could not initialize allreduce.\n");
}

int
GenerationCount::WaitForGenerationHandler(GenerationCount& lco, unsigned long i)
{
  return lco.waitForGeneration(i);
}

hpx_addr_t
hpx_lco_gencount_new(unsigned long ninplace)
{
  hpx_addr_t gva = HPX_NULL;
  GenerationCount* lco = nullptr;
  try {
    lco = new(ninplace * sizeof(Condition), gva) GenerationCount(ninplace);
    hpx_gas_unpin(gva);
  }
  catch (const LCO::NonLocalMemory&) {
    hpx_call_sync(gva, New, &lco, sizeof(lco), &ninplace);
  }
  LCO_LOG_NEW(gva, lco);
  return gva;
}

void
hpx_lco_gencount_inc(hpx_addr_t gencnt, hpx_addr_t rsync)
{
  hpx_lco_set(gencnt, 0, NULL, HPX_NULL, rsync);
}


hpx_status_t
hpx_lco_gencount_wait(hpx_addr_t gva, unsigned long i)
{
  GenerationCount *lva = nullptr;
  if (hpx_gas_try_pin(gva, (void**)&lva)) {
    hpx_status_t status = lva->waitForGeneration(i);
    hpx_gas_unpin(gva);
    return status;
  }
  return hpx_call_sync(gva, WaitForGeneration, NULL, 0, &i);
}

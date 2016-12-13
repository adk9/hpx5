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

#include "Thread.h"
#include "../common/asm.h"
#include "libhpx/parcel.h"
#include <new>

namespace {
using libhpx::scheduler::Thread;

/// A structure describing the initial frame on a stack.
///
/// This must match the transfer.S asm file usage.
///
/// This should be managed in an asm-specific manner.
class [[ gnu::packed ]] TransferFrame
{
 public:
  TransferFrame(hpx_parcel_t*p, Thread::Entry f)
    : lr_(align_stack_trampoline),
      cr_(),
      r2_(nullptr),
      r3_(),
      r14_(f),
      r15_(p),
      regs_{nullptr},
      VFRegs_{nullptr},
      top_r14_(nullptr),
      top_lr_(nullptr)
  {
  }

 private:
  void     (*lr_)(void);         //!< return address
  void             *cr_;         //! CR2-CR4 are non volatile registers
  void             *r2_;         //!< TOC Pointer
  void             *r3_;
  Thread::Entry    r14_;         //!< Function
  void            *r15_;         //!> the parcel that is passed to f
  void       *regs_[16];         //!< r16-r31
  void     *VFRegs_[18];         //!< fpr14-fpr31
  void        *top_r14_;
  void (*top_lr_)(void);
};
}

void
Thread::initTransferFrame(Entry f)
{
  void *addr = top() - sizeof(TransferFrame) - 32;
  sp_ = new(addr) TransferFrame(parcel_, f);
}

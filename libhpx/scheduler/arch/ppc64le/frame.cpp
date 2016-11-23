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
    : r1_(),
      r2_(),
      r14_(f),
      r15_(p),
      regs_(),
      r31_(),
      lr_(),
      vfp_alignment_(),
      fpscr_(),
      VFRegs_(),
      top_r14_(nullptr),
      top_lr_(nullptr)
  {
  }

 private:
  void             *r1_;         //!< Stack pointer
  void             *r2_;         //!< TOC
  Thread::Entry    r14_;         //!< hold the entry function
  void            *r15_;         //!< hold the parcel
  void       *regs_[15];         //!< r16-r30
  void            *r31_;         //!< Frame pointer
  void     (*lr_)(void);         //!< return address
  void  *vfp_alignment_;
  void          *fpscr_;
  void      *VFRegs_[8];
  void        *top_r14_;
  void (*top_lr_)(void);
};
}

void
Thread::initTransferFrame(Entry f)
{
  void *addr = top() - sizeof(TransferFrame);
  sp_ = new(addr) TransferFrame(parcel_, f);
}

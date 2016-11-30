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
    : lr_(f),
      r2_(nullptr),
      r3_(p),
      regs_{nullptr},
      r31_(nullptr),
      vfp_alignment_(nullptr),
      fpscr_(nullptr),
      VFRegs_{nullptr},
      top_r14_(nullptr),
      top_lr_(nullptr)
  {
  }

 private:
  Thread::Entry     lr_;         //!< return address
  void             *r2_;         //!< TOC Pointer
  hpx_parcel_t     *r3_;         //!< Parcel
  void       *regs_[17];         //!< r14-r30
  void            *r31_;         //!< Frame pointer
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

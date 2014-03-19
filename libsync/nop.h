/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/
#ifndef HPX_LIBSYNC_NOP_H_
#define HPX_LIBSYNC_NOP_H_

/* This file defines an interface to nop. */
#include "attributes.h"
#include "sync/sync.h"

/// ----------------------------------------------------------------------------
/// Your basic no-op.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void sync_nop(void);


/// ----------------------------------------------------------------------------
/// An mwait style no-op.
///
/// This has no-op semantics, but could possibly be smarter in a system that has
/// an intelligent wait state, like x86 MONITOR/MWAIT.
/// ----------------------------------------------------------------------------
HPX_INTERNAL void sync_nop_mwait(SYNC_ATOMIC(void*));


#endif /* HPX_LIBSYNC_NOP_H_ */

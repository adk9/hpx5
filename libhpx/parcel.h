// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_PARCEL_H
#define LIBHPX_PARCEL_H

#include "hpx.h"

struct thread;                                  // a user-level thread

HPX_INTERNAL int parcel_init_module(void);
HPX_INTERNAL void parcel_fini_module(void);

/// ----------------------------------------------------------------------------
/// The hpx_parcel structure is what the user-level interacts with.
///
/// The layout of this structure is both important and subtle. The go out onto
/// the network directly, hopefully without being copied in any way. We make sure
/// that the relevent parts of the parcel (i.e., those that need to be sent) are
/// arranged contiguously.
///
/// NB: if you change the layout here, make sure that the network system can deal
/// with it.
///
/// @field    next - intrusive parcel list pointer
/// @field  thread - thread executing this active message
/// @field    data - the data payload associated with this parcel
/// @field    size - the data size in bytes
/// @field  action - the target action identifier
/// @field  target - the target address for parcel_send()
/// @field    cont - the continuation address
/// @field payload - possible in-place payload
/// ----------------------------------------------------------------------------
struct hpx_parcel {
  // these fields are only valid within a locality
  hpx_parcel_t *next;
  struct thread *thread;
  void *data;

  // fields below are sent on wire
  int size;
  hpx_action_t action;
  hpx_addr_t target;
  hpx_addr_t cont;
  char payload[];
};

HPX_INTERNAL void parcel_release(hpx_parcel_t *parcel) HPX_NON_NULL(1);

#endif // LIBHPX_PARCEL_H

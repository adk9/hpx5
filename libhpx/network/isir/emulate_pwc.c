// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
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

#include "libhpx/gas.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "emulate_pwc.h"

/// Emulate a put-with-completion operation.
///
/// This will copy the data buffer into the correct place, and then continue to
/// the completion handler.
HPX_ACTION(isir_emulate_pwc, void *buffer) {
  hpx_parcel_t *p = scheduler_current_parcel();
  if (!p->size) {
    return HPX_SUCCESS;
  }

  gas_t *gas = here->gas;
  void *to;
  if (!gas->try_pin(p->target, &to)) {
    log_net("put-with-completion emulation resend\n");
    return HPX_RESEND;
  }
  memcpy(to, buffer, p->size);
  gas->unpin(p->target);
  return HPX_SUCCESS;
}

/// The reply half of a get-with-completion.
///
/// This abuses the knowledge that local pointers only use 48 bits and have the
/// top 16 bits set to 0. It would be great if this could be an interrupt, but
/// we need to know how much data was sent which requires out-of-band
/// communication.
static HPX_TASK(_gwc_reply, const void *data) {
  static const uint64_t mask = UINT64_MAX >> 16;
  hpx_addr_t target = hpx_thread_current_target();
  void *to = (void*)(mask & target);
  size_t n = hpx_thread_current_args_size();
  memcpy(to, data, n);
  return HPX_SUCCESS;
}

/// Emulate the remote side of a get-with-completion.
///
/// This means copying n bytes from the local target to @p to through a parcel,
/// and then signaling whatever the lsync should be over there.
static int _gwc_request_handler(size_t n, hpx_addr_t to) {
  void *from = hpx_thread_current_local_target();
  hpx_call_cc(to, _gwc_reply, NULL, NULL, from, n);
}
HPX_ACTION_DEF(PINNED, _gwc_request_handler, isir_emulate_gwc, HPX_SIZE_T,
               HPX_ADDR);

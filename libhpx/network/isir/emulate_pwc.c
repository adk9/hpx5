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

/// Emulate the remote side of a get-with-completion.
///
/// This will copy the requested data into a parcel and send it back to the
/// correct source location using the pwc emulation, which does the correct
/// thing.
static int _emulate_gwc_handler(size_t n, hpx_addr_t to, hpx_addr_t complete) {
  hpx_parcel_t *p = scheduler_current_parcel();

  gas_t *gas = here->gas;
  void *from;
  if (!gas->try_pin(p->target, &from)) {
    log_net("get-with-completion emulation resend\n");
    return HPX_RESEND;
  }

  // hpx_call copies the data out of the buffer and into the parcel
  // synchronously, so we can unpin that buffer as soon as the call returns.
  int e = hpx_call(to, isir_emulate_pwc, complete, from, n);
  gas->unpin(p->target);
  return e;
}
HPX_ACTION_DEF(DEFAULT, _emulate_gwc_handler, isir_emulate_gwc, HPX_SIZE_T,
               HPX_ADDR, HPX_ADDR);

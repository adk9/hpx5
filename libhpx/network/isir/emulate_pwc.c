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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "libhpx/gas.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "emulate_pwc.h"

hpx_action_t isir_emulate_pwc = 0;
hpx_action_t isir_emulate_gwc = 0;


/// Emulate a put-with-completion operation.
///
/// This will copy the data buffer into the correct place, and then
static int _emulate_pwc_handler(void *buffer) {
  hpx_parcel_t *p = scheduler_current_parcel();
  if (!p->size) {
    return HPX_SUCCESS;
  }

  gas_t *gas = here->gas;
  void *to;
  if (!gas->try_pin(p->target, &to)) {
    dbg_log_net("put-with-completion emulation resend\n");
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
static int _emulate_gwc_handler(struct isir_emulate_gwc_args *args) {
  hpx_parcel_t *p = scheduler_current_parcel();

  gas_t *gas = here->gas;
  void *from;
  if (!gas->try_pin(p->target, &from)) {
    dbg_log_net("get-with-completion emulation resend\n");
    return HPX_RESEND;
  }

  size_t n = args->n;
  hpx_addr_t to = args->to;
  hpx_addr_t complete = args->complete;

  // hpx_call copies the data out of the buffer and into the parcel
  // synchronously, so we can unpin that buffer as soon as the call returns.
  int e = hpx_call(to, isir_emulate_pwc, from, n, complete);
  gas->unpin(p->target);
  return e;
}


static void HPX_CONSTRUCTOR _register_actions(void) {
  HPX_REGISTER_ACTION(&isir_emulate_pwc, _emulate_pwc_handler);
  HPX_REGISTER_ACTION(&isir_emulate_gwc, _emulate_gwc_handler);
}

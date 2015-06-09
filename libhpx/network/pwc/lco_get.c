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

#include <libhpx/scheduler.h>
#include "pwc.h"
#include "xport.h"

typedef struct {
  int rank;
  hpx_parcel_t *p;
  size_t n;
  void *rva;
  xport_key_t key;
} _pwc_lco_get_args_t;

// /// Perform a remote get operation on an LCO.
// static int
// _pwc_lco_get_handler(_pwc_lco_get_args_t *args, size_t n) {
//   hpx_addr_t lco = hpx_thread_current_target();



//   pwc_network_t *pwc = (pwc_network_t*)here->network;
//   // use the transport to perform the put-with-completion
//   xport_op_t op = {
//     .rank = args->rank,
//     .n = args->n,
//     .dest = args->rva,
//     .dest_key = args->key,
//     .src = buffer,
//     .src_key = pwc->xport->key_find_ref(pwc->xport, p, args->n),
//     .lop = command_pack(_rendezvous_launch, (uintptr_t)p),
//     .rop = command_pack(release_parcel, (uintptr_t)args->p)
//   };
//   int e = pwc->xport->pwc(&op);
//   dbg_check(e, "could not issue get during rendezvous parcel\n");
//   return HPX_SUCCESS;
// }
// HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _pwc_lco_get, _pwc_lco_get_handler,
//            HPX_POINTER, HPX_SIZE_T);

// /// Create a parcel to perform a remote LCO get.
// static hpx_parcel_t *
// _pwc_create_lco_get(hpx_addr_t lco, size_t n, void *out) {
//   _pwc_lco_get_args_t args = {
//     .rank = here->rank,
//     .p = scheduler_current_parcel(),
//     .n = n,
//     .rva = out
//   };

//   hpx_parcel_t *p = parcel_create(lco, _pwc_lco_get, HPX_NULL, HPX_ACTION_NULL,
//                                   2, &args, &n);
//   return p;
// }

int
pwc_lco_get(void *obj, hpx_addr_t lco, size_t n, void *out) {
  return scheduler_wait_launch_through(NULL, HPX_NULL);
}

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

#include <stdio.h>
#include <inttypes.h>
#include <hpx/hpx.h>

static hpx_action_t _main       = 0;
static hpx_action_t _initDomain = 0;

typedef struct Domain {
  int nDoms;
  int rank;
} Domain;

typedef struct {
  int index;
  int nDoms;
} InitArgs;
 
#define NDOMS 8

/// Initialize a domain.
static int _initDomain_action(const InitArgs *args)
{
  // Get the address this parcel was sent to, and map it to a local address---if
  // this fails then the message arrived at the wrong place due to AGAS
  // movement, so resend the parcel.
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // Update the domain with the argument data.
  ld->rank = args->index;
  ld->nDoms = args->nDoms;

  // make sure to unpin the domain, so that AGAS can move it if it wants to
  hpx_gas_unpin(local);
  printf("Initialized domain %u\n", args->index);
  // return success---this triggers whatever continuation was set by the parcel
  // sender
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_alloc_cyclic(NDOMS, sizeof(Domain), 0);

  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(NDOMS);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = NDOMS; i < e; ++i) {
    // Allocate a parcel with enough inline buffer space to store an InitArgs
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(InitArgs));
    // Get access to the parcel's buffer, and fill it with the necessary data.
    InitArgs *init = hpx_parcel_get_data(p);
    init->index = i;
    init->nDoms = NDOMS;

    // set the target address and action for the parcel
    hpx_addr_t targetAddr = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_parcel_set_target(p, targetAddr);
    hpx_parcel_set_action(p, _initDomain);

    assert(hpx_parcel_get_target(p) == targetAddr);
    assert(hpx_parcel_get_action(p) == _initDomain);

    // set the continuation target and action for the parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    assert(hpx_parcel_get_cont_target(p) == done);
    assert(hpx_parcel_get_cont_action(p) == hpx_lco_set_action);

    // and send the parcel---we used the parcel's buffer directly so we don't
    // have to wait for local completion (hence HPX_NULL), also this transfers
    // ownership of the parcel back to the runtime so we don't need to release
    // it
    hpx_parcel_send(p, HPX_NULL);
  }

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // and free the domain
  hpx_gas_free(domain, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_initDomain_action, &_initDomain);

  return hpx_run(&_main, NULL, 0);
}

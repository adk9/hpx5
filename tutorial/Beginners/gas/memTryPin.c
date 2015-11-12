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
static int _initDomain_action(const InitArgs *args, size_t size)
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

static int _main_action(void *args, size_t size) {
  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_alloc_cyclic(NDOMS, sizeof(Domain), 0);

  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(NDOMS);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = NDOMS; i < e; ++i) {

    // hpx_call() will copy this
    InitArgs init = {
      .index = i,
      .nDoms = NDOMS
    };

    // compute the offset for this domain
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));

    // and send the initDomain action, with the done LCO as the continuation
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // and free the domain
  hpx_gas_free(domain, HPX_NULL);

  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _initDomain, _initDomain_action, HPX_POINTER, HPX_SIZE_T);

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}

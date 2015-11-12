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

// Program that computes transpose of a matrix using
// hpx_lco_alltoall.

#include <stdio.h>
#include <stdlib.h>
#include <hpx/hpx.h>

static hpx_action_t _main      = 0;
static hpx_action_t _init      = 0;
static hpx_action_t _check_sum = 0;

int nDoms                   = 8;

typedef struct {
  int  index;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

typedef struct Domain {
  int index;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} Domain;

static int _init_action(const InitArgs *args, size_t size) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index    = args->index;
  ld->complete = args->complete;
  // Record the alltoall value
  ld->newdt = args->newdt;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _check_sum_action(int *args, size_t size) {
  hpx_addr_t target = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(target, (void**)&ld))
    return HPX_RESEND;

  // Create a random array of elements on all processes.
  int gnewdt[nDoms];
  for (int k = 0; k < nDoms; k++) {
    gnewdt[k] = (k+1) + ld->index * nDoms;
  }

  hpx_lco_alltoall_setid(ld->newdt, ld->index, nDoms * sizeof(int),
                         gnewdt, HPX_NULL, HPX_NULL);

  // Get the gathered value, and print it.
  int newdt[nDoms];
  hpx_lco_alltoall_getid(ld->newdt, ld->index, sizeof(newdt), &newdt);

  printf("\nIndex: = #%d : ", ld->index);
  for (int i = 0; i < nDoms; i++)
    printf(" %d ", newdt[i]);
  printf("\n\n");

  hpx_lco_set(ld->complete, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static int _main_action(void *args, size_t size) {
  hpx_addr_t domain   = hpx_gas_alloc_cyclic(nDoms, sizeof(Domain), 0);
  hpx_addr_t done     = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);
  hpx_addr_t newdt = hpx_lco_alltoall_new(nDoms, nDoms * sizeof(int));

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .index  = i,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t remote = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(remote, _init, done, &init, sizeof(init));
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  for (int i = 0, e = nDoms; i < e; ++i) {
    hpx_addr_t remote = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(remote, _check_sum, HPX_NULL, NULL, 0);
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);
  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  // Seed the random number generator to get different results each time.
  srand(time(NULL));

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _init, _init_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _check_sum, _check_sum_action, HPX_POINTER, HPX_SIZE_T);

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}

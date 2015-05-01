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

//****************************************************************************
// Example code - hpx_thread_current_target  -- Get the target of the current
//                target. The target of the thread is the destination that a 
//                parcel was sent to spawn the current thread.
//****************************************************************************
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <hpx/hpx.h>

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static hpx_action_t _main       = 0;
static hpx_action_t _initArray   = 0;

/// Initialize a array.
static int _initArray_action(void *args, size_t size)
{
  // Get the address this parcel was sent to, and map it to a local 
  // address---if this fails then the message arrived at the wrong 
  // place, so resend the parcel.
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, size);

  // make sure to unpin the target
  hpx_gas_unpin(target);

  printf("Initialized the array with: ");
  for (int i = 0; i < (sizeof(block) / sizeof(block[0])); ++i)
    printf("%" PRIu64 " ", local[i]);
  printf("\n");

  // return success---this triggers whatever continuation was set 
  // by the parcel sender
  return HPX_SUCCESS;
}

static int _main_action(void *args, size_t n) {
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank+1)%size;
 
  // Allocate the domain array
  hpx_addr_t global = hpx_gas_alloc_cyclic(size, sizeof(block), 0);
  hpx_addr_t remote = hpx_addr_add(global, peerid * sizeof(block), sizeof(block));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, _initArray, done, block, sizeof(block));

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // and free the domain
  hpx_gas_free(global, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _initArray, _initArray_action, HPX_POINTER, HPX_SIZE_T);

  return hpx_run(&_main, NULL, 0);
}

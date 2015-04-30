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

// This test demonstrates the correctness of hpx_gas_memget function.
// 1. Initializes the content of array "masterCopy". Then we copy the contents
// to its local array "localCopy", perform error checking
// 2. Reinitializes the content of array "masterCopy" with different values.
// perform multiple get from "masterCopy" to local array "localCopy" and
// perform the error checking.

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <hpx/hpx.h>

static  hpx_action_t _main       = 0;
static  hpx_action_t _init_array = 0;

// Note: to run it with the larger SIZE, increase the --hpx-stacksize. It is
// defaulted to 64k, else you will run into stack overrun.

#define SIZE 1000
#define LOCALCOPY 5

static uint64_t masterCopy[SIZE];
uint64_t localCopy[LOCALCOPY][SIZE];

static int _init_array_action(size_t size, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank + 1) % size;

  // Initialize the master copy
  for (int i = 0; i < SIZE; i++) 
    masterCopy[i] = (uint64_t)(i);
  
  hpx_addr_t data = hpx_gas_alloc_cyclic(size, sizeof(masterCopy), 0);
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(masterCopy), sizeof(masterCopy));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, _init_array, done, masterCopy, sizeof(masterCopy));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
 
  // Copy the masterCopy's content to local array
  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memget(&localCopy[0], remote, sizeof(masterCopy), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  // Perform the error checking
  for (int i = 0; i < SIZE; i++) 
    assert(localCopy[0][i] == masterCopy[i]);

  for (int i = 0; i < SIZE; i++)
    masterCopy[i] = (uint64_t)(((uint64_t)(i) * (uint64_t)(10)) + (uint64_t)(1));

  hpx_addr_t rfut = hpx_lco_future_new(0);
  hpx_call(remote, _init_array, rfut, masterCopy, sizeof(masterCopy));
  hpx_lco_wait(rfut);
  hpx_lco_delete(rfut, HPX_NULL);

  // Perform multiple hpx_gas_memget
  for (int i = 0; i < LOCALCOPY; i++) {
    hpx_addr_t rfut1 = hpx_lco_future_new(0);
    hpx_gas_memget(&localCopy[i], remote, sizeof(masterCopy), rfut1);
    hpx_lco_wait(rfut1);
    hpx_lco_delete(rfut1, HPX_NULL);
  }
    
  // Perform error checking
  for (int i = 0; i < LOCALCOPY; i++)
    for (int j = 0; j < SIZE; j++)
      assert(localCopy[i][j] == (uint64_t)((uint64_t)(j) * (uint64_t)(10) + (uint64_t)(1))); 

  printf("hpx_gas_memget succeeded for size = %d\n", SIZE);

  hpx_gas_free(data, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _init_array, _init_array_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}

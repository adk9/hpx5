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
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <hpx/hpx.h>
#include <unistd.h>

static  hpx_action_t _main       = 0;
static  hpx_action_t _init_array = 0;

#define BLOCK_COUNT 1000
#define BLOCK_SIZE BLOCK_COUNT * sizeof(uint64_t)

static int _init_array_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  unsigned block_size = hpx_thread_current_args_size();
  memcpy(local, args, block_size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  uint64_t *local;
  int size = HPX_LOCALITIES;

  uint64_t *src = calloc(BLOCK_SIZE, sizeof(uint64_t));

  for (int i = 0; i < size; i++)
    for (int j = 0; j < BLOCK_COUNT; j++)
      src[i * BLOCK_COUNT + j] = (uint64_t)(i);

  hpx_addr_t data = hpx_gas_alloc_cyclic(size, BLOCK_SIZE, 0);
  
  hpx_addr_t done = hpx_lco_and_new(size);
  for (int i = 0; i < size; i++) {
    hpx_addr_t remote_block = hpx_addr_add(data, i * BLOCK_SIZE, BLOCK_SIZE);
    hpx_call(remote_block, _init_array, done, &src[i * BLOCK_COUNT],
             BLOCK_SIZE);
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // At this point, there should be one block of data at each locality,
  // and all data in that block should be equal to that locality's id
  // i.e. local_block[i] == HPX_LOCALITY_ID is true for all i < BLOCK_SIZE

  // At locality 0, all values should be 0
  hpx_gas_try_pin(data, (void**)&local);
  for (int i = 0; i < BLOCK_COUNT; i++)
    assert(local[i] == 0);
  hpx_gas_unpin(data);

  // Now we will copy the block from the locality with the highest id
  // to locality 0.
  unsigned last_rank = (size - 1);
  hpx_addr_t remote_data = hpx_addr_add(data, 
                                        last_rank * BLOCK_SIZE,
                                        BLOCK_SIZE);
  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memcpy(data, remote_data, BLOCK_SIZE, completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  // Now all values at locality 0 should be equal to size - 1
  hpx_gas_try_pin(data, (void**)&local);
  for (int i = 0; i < BLOCK_COUNT; i++)
    assert(local[i] == size - 1);
  hpx_gas_unpin(data);

  printf("hpx_gas_memcpy succeeded for size = %zu\n", BLOCK_SIZE);

  hpx_gas_free(data, HPX_NULL);
  free(src);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, 0, _main, _main_action);
  HPX_REGISTER_ACTION(HPX_DEFAULT, 0, _init_array, _init_array_action);

  return hpx_run(&_main, NULL, 0);
}

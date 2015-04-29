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

static  hpx_action_t _main   = 0;
static  hpx_action_t _verify = 0;

static uint64_t block[] = {
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
  22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

static int _verify_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  bool result = false;
  const size_t BLOCK_ELEMS = sizeof(block) / sizeof(block[0]);
  for (int i = 0; i < BLOCK_ELEMS; ++i)
    result |= (local[i] != block[i]);

  hpx_gas_unpin(target);
  HPX_THREAD_CONTINUE(result);
}

static int _main_action(void *args) {
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank + 1) % size;

  hpx_addr_t data = hpx_gas_alloc_cyclic(size, sizeof(block), 0);
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(block), sizeof(block)); 

  hpx_addr_t rfut = hpx_lco_future_new(0);
  hpx_gas_memput(remote, block, sizeof(block), HPX_NULL, rfut);
  hpx_lco_wait(rfut);
  hpx_lco_delete(rfut, HPX_NULL);

  bool output = false;
  hpx_call_sync(remote, _verify, &output, sizeof(output), NULL, 0);
  assert(output == false);

  printf("hpx_gas_memput succeeded\n");

  hpx_gas_free(data, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, 0, _main, _main_action);
  HPX_REGISTER_ACTION(HPX_DEFAULT, 0, _verify, _verify_action);

  return hpx_run(&_main, NULL, 0);
}

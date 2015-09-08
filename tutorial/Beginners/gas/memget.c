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

static  hpx_action_t _main       = 0;
static  hpx_action_t _init_array = 0;

// Note: to run it with the larger SIZE, increase the --hpx-stacksize. It is
// defaulted to 64k, else you will run into stack overrun.

#define SIZE 1000
static uint64_t src[SIZE];

static int _init_array_action(void *args, size_t size) {
  hpx_addr_t target = hpx_thread_current_target();
  uint64_t *local = NULL;
  if (!hpx_gas_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(local, args, size);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}

static int _main_action(void *args, size_t n) {
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peerid = (rank + 1) % size;

  for (int i = 0; i < SIZE; i++) 
    src[i] = (uint64_t)(i);
  
  hpx_addr_t data = hpx_gas_alloc_cyclic(size, sizeof(src), 0);
  hpx_addr_t remote = hpx_addr_add(data, peerid * sizeof(src), sizeof(src));

  hpx_addr_t done = hpx_lco_future_new(sizeof(void*));
  hpx_call(remote, _init_array, done, src, sizeof(src));
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
 
  uint64_t dst[SIZE];
  memset(dst, 0xFF, SIZE);

  hpx_addr_t completed = hpx_lco_future_new(0);
  hpx_gas_memget(&dst, remote, sizeof(src), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  for (int i = 0; i < SIZE; i++) 
    assert(dst[i] == src[i]);

  printf("hpx_gas_memget succeeded for size = %d\n", SIZE);

  hpx_gas_free(data, HPX_NULL);
  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _init_array, _init_array_action, HPX_POINTER, HPX_SIZE_T);

  e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}

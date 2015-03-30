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
#include <hpx/hpx.h>

static  hpx_action_t _main       = 0;
static  hpx_action_t _pin        = 0;

static int _pin_action(void *args) {
  printf("Populating the data\n");

  hpx_addr_t local = hpx_thread_current_target();
  int *source = NULL;
  if (!hpx_gas_try_pin(local, (void**)&source))
    return HPX_RESEND;

  for (int i = 0; i < 10; i++) {
    source[i] = i;
    printf("Source[i] = '%d'\n", source[i]);
  }

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(void *args) {

  // malloc(0) returns "either" a null pointer or a unique pointer that can be 
  // successfully passed to free()"."
  hpx_addr_t local = hpx_gas_alloc_local(0, 0);
  hpx_addr_t done = hpx_lco_future_new(sizeof(double));
  hpx_call(local, _pin, done, NULL, 0);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_free(local, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
   
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_pin_action, &_pin);

  return hpx_run(&_main, NULL, 0);
}

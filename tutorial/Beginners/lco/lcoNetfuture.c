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

static  hpx_action_t _main = 0;
static  hpx_action_t _get  = 0;
#define BUFFER_SIZE 128

struct buffer {
  char data[BUFFER_SIZE];
};

typedef struct {
  int ranks;
  hpx_netfuture_t message;
} args_t;

static int _get_action(args_t *args) {
  hpx_addr_t msg = hpx_gas_alloc_local(sizeof(struct buffer), 0);
  struct buffer *hello_msg;
  hpx_gas_try_pin(msg, (void**)&hello_msg);

  snprintf(hello_msg->data, BUFFER_SIZE, "Hello World!");
  printf("Message set = '%s'\n", hello_msg->data);

  hpx_addr_t lsync = hpx_lco_future_new(0);
  hpx_lco_netfuture_setat(args->message, 0, BUFFER_SIZE, msg, lsync);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);

  struct buffer *result_msg;
  hpx_addr_t msg_set = hpx_gas_alloc_local(sizeof(struct buffer), 0);
  msg_set = hpx_lco_netfuture_getat(args->message, 0, BUFFER_SIZE);
  hpx_gas_try_pin(msg_set, (void**)&result_msg);
  printf("Message got = '%s'\n", result_msg->data);

  hpx_gas_unpin(msg_set);

  hpx_lco_netfuture_emptyat(args->message, 0, HPX_NULL);
  hpx_gas_unpin(msg);

  return HPX_SUCCESS;
}

static int _main_action(args_t *args) {
  hpx_netfutures_init(NULL);

  hpx_addr_t done = hpx_lco_future_new(sizeof(uint64_t));
  hpx_netfuture_t base = hpx_lco_netfuture_new_all(1, BUFFER_SIZE);
  printf("Future allocated\n");

  args->message = base;

  hpx_call(HPX_HERE, _get, done, args, sizeof(*args));
 
  hpx_lco_wait(done); 
  hpx_netfutures_fini();
  hpx_lco_delete(done, HPX_NULL);  

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }
  
  args_t args = {
    .ranks = hpx_get_num_ranks(),
  };
 
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_get_action, &_get);

  return hpx_run(&_main, &args, sizeof(args));
}

// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"

#define T int

static T value;

static hpx_action_t set_value = 0;
static hpx_action_t get_value = 0;
static hpx_action_t allreduce = 0;

static T
sum(T count, T values[count]) {
  T total = 0;
  for (int i = 0; i < count; ++i, total += values[i])
    ;
  return total;
}

static int
action_get_value(void *args) {
  HPX_THREAD_CONTINUE(value);
}

static int
action_set_value(void *args) {
  value = *(T*)args;
  printf("At rank %d received value %lld\n", hpx_get_my_rank(), (long long)value);
  return HPX_SUCCESS;
}

static int
action_allreduce(void *unused) {
  int num_ranks = HPX_LOCALITIES;
  int my_rank = HPX_LOCALITY_ID;
  assert(my_rank == 0);

  T          values[num_ranks];
  void      *addrs[num_ranks];
  int        sizes[num_ranks];
  hpx_addr_t futures[num_ranks];

  for (int i = 0; i < num_ranks; ++i) {
    addrs[i] = &values[i];
    sizes[i] = sizeof(T);
    futures[i] = hpx_lco_future_new(sizeof(T));
    hpx_call(HPX_THERE(i), get_value, NULL, 0, futures[i]);
  }

  hpx_lco_get_all(num_ranks, futures, sizes, addrs, NULL);

  value = sum(num_ranks, values);

  for (int i = 0; i < num_ranks; ++i) {
    hpx_lco_delete(futures[i], HPX_NULL);
    futures[i] = hpx_lco_future_new(0);
    hpx_call(HPX_THERE(i), set_value, &value, sizeof(value), futures[i]);
  }

  hpx_lco_get_all(num_ranks, futures, sizes, addrs, NULL);

  for (int i = 0; i < num_ranks; ++i)
    hpx_lco_delete(futures[i], HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char** argv) {
  
  hpx_config_t config = HPX_CONFIG_DEFAULTS;

  switch (argc) {
   default:
    fprintf(stderr, "Usage: allreduce [optional THREADS].\n");
    return -1;
   case (2):
    config.threads = atoi(argv[2]);
    break;
   case (1):
    break;
  }

  int success = hpx_init(&config);
  if (success != 0) {
    printf("Error %d in hpx_init!\n", success);
    exit(-1);
  }

  // register action for parcel
  set_value = hpx_register_action("set_value", action_set_value);
  get_value = hpx_register_action("get_value", action_get_value);
  allreduce = hpx_register_action("allreduce", action_allreduce);

  // Initialize the values that we want to reduce
  value = hpx_get_my_rank();

  return hpx_run(allreduce, NULL, 0);
}

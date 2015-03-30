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

// Program that computes the average of an array of elements in parallel using
// hpx_lco_allreduce.

#include <stdio.h>
#include <stdlib.h>
#include <hpx/hpx.h>

static hpx_action_t _main      = 0;
static hpx_action_t _init      = 0;
static hpx_action_t _check_sum = 0;

int num_elements_per_proc   = 10;
int nDoms                   = 8;

typedef struct {
  int        index;
  hpx_addr_t complete;
  hpx_addr_t gsum;
} InitArgs;

typedef struct Domain {
  int        index;
  hpx_addr_t complete;
  hpx_addr_t gsum;
} Domain;

// Creates an array of random numbers. Each number has a value from 0 - 1
float *create_rand_nums(int num_elements) {
  float *rand_nums = (float *)malloc(sizeof(float) * num_elements);
  assert(rand_nums != NULL);
  int i;
  for (i = 0; i < num_elements; i++) {
    rand_nums[i] = (rand() / (float)RAND_MAX);
  }
  return rand_nums;
}

static void initdouble(double *input, const size_t size) {
  assert(sizeof(double) == size);
  *input = 0.0;
}

static void sumdouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  *output += *input;
  return;
}

static int _init_action(const InitArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->index    = args->index;
  ld->complete = args->complete;

  // Record the allreduce value
  ld->gsum = args->gsum;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _check_sum_action(int *args) {
  double sum = 0.0, gsum = 0.0;
  int ranks = HPX_LOCALITIES;

  hpx_addr_t target = hpx_thread_current_target();
  Domain *ld;
  if (!hpx_gas_try_pin(target, (void**)&ld))
    return HPX_RESEND;

  // Create a random array of elements on all processes.
  srand(ranks);
  float *rand_nums = create_rand_nums(num_elements_per_proc);

  // Sum the numbers locally
  for (int i = 0; i < num_elements_per_proc; i++) {
    sum += rand_nums[i];
  }

  free(rand_nums);

  // Print the random numbers on each process
  printf("Local sum  %g, avg = %g\n", sum, sum / num_elements_per_proc);

  hpx_lco_set(ld->gsum, sizeof(double), &sum, HPX_NULL, HPX_NULL);
  hpx_lco_get(ld->gsum, sizeof(double), &gsum);

  if (ld->index == 0)
    printf("Global sum = %g\n", gsum);

  hpx_lco_set(ld->complete, 0, NULL, HPX_NULL, HPX_NULL);
  hpx_gas_unpin(target);

  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  hpx_addr_t domain   = hpx_gas_alloc_cyclic(nDoms, sizeof(Domain), 0);
  hpx_addr_t done     = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);
  hpx_addr_t gsum = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(double),
                                         (hpx_monoid_id_t)initdouble,
                                         (hpx_monoid_op_t)sumdouble);

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .index    = i,
      .complete = complete,
      .gsum = gsum
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
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  // Seed the random number generator to get different results each time.
  srand(time(NULL));

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_init_action, &_init);
  HPX_REGISTER_ACTION(_check_sum_action, &_check_sum);

  return hpx_run(&_main, NULL, 0);
}

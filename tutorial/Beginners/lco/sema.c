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

#define NITER         1000000
#define NUM_THREADS   2
static  hpx_action_t _main = 0;
static  hpx_action_t _add  = 0;

int count = 0;
hpx_addr_t sem;

static int _add_action(int *args) {
  int i, tmp;
  for (i = 0; i < NITER; ++i) {
    hpx_lco_sema_p(sem);
    tmp = count;
    tmp = tmp + 1;
    count = tmp;
    hpx_lco_sema_v_sync(sem);
  }
  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  sem = hpx_lco_sema_new(1);
  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (int i = 0; i < NUM_THREADS; i++)
    hpx_call(HPX_HERE, _add, and, &i, sizeof(i));

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  if (count < 2 * NITER) {
    printf("BOOM! count is [%d], should be %d\n", count, 2*NITER);
  } else {
    printf("OK! count is [%d]\n", count);
  }

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_SIZE_T, HPX_POINTER);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _add, _add_action, HPX_SIZE_T, HPX_POINTER);

  return hpx_run(&_main, NULL, 0);
}

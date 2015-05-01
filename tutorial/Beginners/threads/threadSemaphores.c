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
// Example code - This tutorial illustrates use of HPX semaphores in a thread
// program that performs a dot product. The main data is available to all
// threads through a globally accessible structure. Each thread works on a
// different part of the data. The main thread waits for all the threads to
// complete their computations and then prints the output.
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <hpx/hpx.h>

typedef struct {
  double      *a;
  double      *b;
  double     sum;
  int     veclen;
} DOTDATA;

#define NUM_THREADS                4
#define VECLEN                   100
DOTDATA dotstr;
hpx_addr_t mutex;

static  hpx_action_t _main       = 0;
static  hpx_action_t _dotprod    = 0;

void _dotprod_action(long *arg, size_t size) {
  int i, start, end, len;
  long offset = *arg;
  double mysum, *x, *y;

  len = dotstr.veclen;
  start = offset*len;
  end   = start + len;
  x = dotstr.a;
  y = dotstr.b;

  /*
   Perform the dot product and assign result
   to the appropriate variable in the structure.
  */

  mysum = 0;
  for (i = start; i < end ; i++) {
    mysum += (x[i] * y[i]);
  }
  /*
   Lock a semaphore prior to updating the value in the shared
   structure, and unlock it upon updating.
  */
  hpx_lco_sema_p (mutex);
  dotstr.sum += mysum;
  hpx_lco_sema_v_sync (mutex);

  hpx_thread_exit(HPX_SUCCESS);
}

//****************************************************************************
// @Action which spawns the threads
//****************************************************************************
static int _main_action(int *args, size_t size) {
  long i;
  double *a, *b;

  /* Assign storage and initialize values */
  a = (double*) malloc (NUM_THREADS*VECLEN*sizeof(double));
  b = (double*) malloc (NUM_THREADS*VECLEN*sizeof(double));

  for (i = 0; i < VECLEN * NUM_THREADS; i++) {
    a[i]=1.0;
    b[i]=a[i];
  }

  dotstr.veclen = VECLEN;
  dotstr.a = a;
  dotstr.b = b;
  dotstr.sum=0;

  mutex = hpx_lco_sema_new(1);

  hpx_addr_t and = hpx_lco_and_new(NUM_THREADS);
  for (i = 0; i < NUM_THREADS; i++)
    hpx_call(HPX_HERE, _dotprod, and, &i, sizeof(i));

  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  /* After waiting, print out the results and cleanup */
  printf ("Sum =  %f \n", dotstr.sum);
  free (a);
  free (b);

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _dotprod, _dotprod_action, HPX_POINTER, HPX_SIZE_T);

  return hpx_run(&_main, NULL, 0);
}

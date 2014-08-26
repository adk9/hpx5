//****************************************************************************
// @Filename      04_TestParcel.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Tests the parcel funcitonalities
// @Goal          Goal of this testcase is to test the Parcels
//                1. hpx_parcel_aquire()             
//                2. hpx_parcel_set_target()
//                3. hpx_parcel_set_action()
//                4. hpx_parcel_set_data()
//                5. hpx_parcel_send_sync()                   
//                            
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/21/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

int t04_send_action(void *args) {
  int n = *(int*)args;
  printf("locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
         hpx_get_my_thread_id(), n);

  if (n-- <= 0) {
    printf("terminating.\n");
    return HPX_SUCCESS;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL,sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, t04_send);
  hpx_parcel_set_data(p, &n, sizeof(n));
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- Parcels
//****************************************************************************
START_TEST (test_libhpx_parcel)
{
  int n = 0;
  printf("Starting the parcel test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t completed = hpx_lco_and_new(0);

  hpx_call(HPX_HERE, t04_send, &n, sizeof(n), completed);
  
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_04_TestParcel(TCase *tc) {
  tcase_add_test(tc, test_libhpx_parcel);
}

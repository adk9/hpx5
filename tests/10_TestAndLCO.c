//****************************************************************************
// @Filename      10_TestAndLCO.c 
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - HPX LCO AND
// 
// @Compiler      GCC
// @OS            Linux
// @Description   and.c File Reference
// @Goal          Goal of this testcase is to test the HPX LCO and
//                1. hpx_lco_and_new -- Allocate an and lco
//                2. hpx_lco_and_set -- Join the and.
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
// @Date          09/05/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include "hpx/hpx.h"
#include "tests.h"

//****************************************************************************
// Testcase for and LCO.
//****************************************************************************
int t10_set_action(input_args_t *args) {
  hpx_lco_and_set(args->lco, HPX_NULL);
  return HPX_SUCCESS;
}

void send_lco(hpx_addr_t lco, int dst, size_t size) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  input_args_t *args = hpx_parcel_get_data(p);
  args->lco = lco;
  args->dst = dst;
  args->size = size;
  hpx_parcel_set_action(p, t10_set);
  hpx_parcel_set_target(p, HPX_THERE(dst));
  hpx_parcel_send(p, HPX_NULL);
}

START_TEST (test_libhpx_lco_and)
{
  printf("Starting the HPX and lco test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // Allocate an and LCO. This is synchronous. An and LCO generates an AND
  // gate. Inputs should be >=0;
  hpx_addr_t done = hpx_lco_and_new(1);

  send_lco(done, 1, sizeof(input_args_t));
  hpx_lco_wait(done);
  
  hpx_lco_delete(done, HPX_NULL);
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_10_TestAndLCO(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_and);
}

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
int t10_set_action(void *args) {
  hpx_lco_and_set(*(hpx_addr_t*)args, HPX_NULL);
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_lco_and)
{
  fprintf(test_log, "Starting the HPX and lco test\n");
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // Allocate an and LCO. This is synchronous. An and LCO generates an AND
  // gate. Inputs should be >=0;
  hpx_addr_t lco = hpx_lco_and_new(1);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_call(HPX_HERE, t10_set, done, &lco, sizeof(lco));
  hpx_lco_wait(lco);
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);
  fprintf(test_log, " Elapsed: %g\n", hpx_time_elapsed_ms(t1));
} 
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_10_TestAndLCO(TCase *tc) {
  tcase_add_test(tc, test_libhpx_lco_and);
}

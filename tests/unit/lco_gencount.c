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

// Goal of this testcase is to test the HPX LCO Generation counter
//  1. hpx_lco_gencount_new -- Allocate a new generation counter
//  2. hpx_lco_gencount_inc -- Increment the generation counter.
//  3. hpx_lco_gencount_wait -- Wait for the generation counter 
//  to reach a certain value. 

#include "hpx/hpx.h"
#include "tests.h"

// Testcase for gencount LCO.
static int _increment_handler(size_t n, hpx_addr_t *args) {
  hpx_addr_t addr = *args;

  // Increment the generation counter.
  // Counter to increment and the global address of an LCO signal remote
  // completion.
  printf("Incrementing the generation counter\n");
  hpx_lco_gencount_inc(addr, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _increment,
                  _increment_handler, HPX_SIZE_T, HPX_POINTER);

static int lco_gencount_handler(void) {
  printf("Starting the HPX gencount lco test\n");
  //int ninplace = 4;
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();
  // Allocate a new generation counter. A generation counter allows an 
  // application programmer to efficiently wait for a counter. The input 
  // indicates a bound on the typical number of generations that are 
  // explicitly active
  // hpx_addr_t lco = hpx_lco_gencount_new(4);

  hpx_addr_t lco = hpx_lco_gencount_new(0);
  hpx_addr_t done = hpx_lco_future_new(0);
  //for (int i = 0; i < ninplace; i++)  
    hpx_call(HPX_HERE, _increment, done, &lco, sizeof(lco));

  // Wait for the generation counter to reach a certain value.
  //hpx_lco_gencount_wait(lco, ninplace);
  hpx_lco_gencount_wait(lco, 0);
  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  // hpx_lco_delete(lco, HPX_NULL); 
 
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, lco_gencount, lco_gencount_handler);

TEST_MAIN({
 ADD_TEST(lco_gencount);
});

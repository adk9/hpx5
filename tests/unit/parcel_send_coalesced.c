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
#include <hpx/topology.h>
#include "tests.h"



/* static int _set_action(void *args, size_t size) { */
/*   hpx_lco_and_set(*(hpx_addr_t*)args, HPX_NULL); */
/*   return HPX_SUCCESS; */
/* } */

static int _set_action(hpx_addr_t args) {
  printf("Setting the lco in the set handler\n");
  assert(args);
  hpx_lco_and_set(args, HPX_NULL);
  printf("And lco set successfully\n");
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _set, _set_action, HPX_ADDR);
//HPX_COALESCED
static int _checkcoalescing_action(void) {
  hpx_addr_t lco = hpx_lco_and_new(hpx_get_num_ranks());
  hpx_addr_t done = hpx_lco_future_new(0);

  //hpx_call(HPX_HERE, _set, done, &lco, sizeof(lco));
  hpx_bcast(_set, HPX_NULL, done, &lco);

  hpx_lco_wait(lco);
  hpx_lco_wait(done);

  hpx_lco_delete(done, HPX_NULL);
  hpx_lco_delete(lco, HPX_NULL);

  printf("LCO Set succeeded\n");

  return HPX_SUCCESS;
  //hpx_exit(HPX_SUCCESS);
}

static HPX_ACTION(HPX_DEFAULT, 0, _checkcoalescing, _checkcoalescing_action);


TEST_MAIN({
  ADD_TEST(_checkcoalescing, 0);
});



/* #include <stdio.h> */
/* #include <hpx/hpx.h> */
/* #include "tests.h" */
/* //static  hpx_action_t _lco = 0; */
/* //static  hpx_action_t _set  = 0; */

/* static int _set_action(void *args, size_t size) { */
/*   hpx_lco_and_set(*(hpx_addr_t*)args, HPX_NULL); */
/*   return HPX_SUCCESS; */
/* } */
/* static  HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED | HPX_COALESCED, _set, _set_action, HPX_POINTER, HPX_SIZE_T); */

/* static int _lco_action(void *args, size_t size) { */
/*   hpx_addr_t lco = hpx_lco_and_new(1); */
/*   hpx_addr_t done = hpx_lco_future_new(0); */

/*   hpx_call(HPX_HERE, _set, done, &lco, sizeof(lco)); */

/*   hpx_lco_wait(lco); */
/*   hpx_lco_wait(done); */

/*   hpx_lco_delete(done, HPX_NULL); */
/*   hpx_lco_delete(lco, HPX_NULL); */

/*   printf("LCO Set succeeded\n"); */

/*   hpx_exit(HPX_SUCCESS); */
/* } */

/* /\* int main(int argc, char *argv[]) { *\/ */
/* /\*   int e = hpx_init(&argc, &argv); *\/ */
/* /\*   if (e) { *\/ */
/* /\*     fprintf(stderr, "HPX: failed to initialize.\n"); *\/ */
/* /\*     return e; *\/ */
/* /\*   } *\/ */

/* static  HPX_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _lco, _lco_action, HPX_POINTER, HPX_SIZE_T); */

/* TEST_MAIN({ */
/*   ADD_TEST(_lco, 0); */
/* }); */


/*   int success = hpx_run(&_main, NULL, 0); */
/*   hpx_finalize(); */
/*   return success; */
/* } */





/* #include <stdio.h> */
/* #include <stdlib.h> */
/* #include <math.h> */
/* #include <time.h> */
/* #include <libhpx/libhpx.h> */
/* #include <libhpx/locality.h> */
/* #include "tests.h" */

/* static int _reduce_handler(hpx_addr_t args) { */
/*   printf("Setting the lco in the reduce one handler\n"); */
/*   hpx_lco_and_set(args, HPX_NULL); */
/*   return HPX_SUCCESS; */
/* } */
/* static HPX_ACTION(HPX_DEFAULT, HPX_COALESCED, _reduce, _reduce_handler, HPX_ADDR); */

/* static int lco_reduce_handler(void) { */
/*   printf("In lco reduce one handler\n"); */
/*   hpx_addr_t and = hpx_lco_and_new(here->ranks); */
/*   hpx_bcast(_reduce, HPX_NULL, HPX_NULL, &and); */
/*   hpx_lco_wait(and); */
/*   hpx_lco_delete(and, HPX_NULL); */

/*   /\* hpx_addr_t and2 = hpx_lco_and_new(here->config->coalescing_buffersize); *\/ */
/*   /\* hpx_bcast_rsync(_reduce, &and2, sizeof(and2)); *\/ */
/*   /\* hpx_lco_wait(and2); *\/ */
/*   /\* hpx_lco_delete(and2, HPX_NULL); *\/ */

/*   return HPX_SUCCESS; */
/* } */
/* static HPX_ACTION(HPX_DEFAULT, 0, lco_reduce, lco_reduce_handler); */


/* TEST_MAIN({ */
/*   ADD_TEST(lco_reduce, 0); */
/* }); */

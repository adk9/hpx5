
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Versioned Gates
  12_gate.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#include "hpx.h"
#include "tests.h"

/* Globals */
static hpx_future_t * fut;
static hpx_gate_t * gate;


/*
 --------------------------------------------------------------------
  TEST HELPER: predicate function for gate_allreduce_worker
 --------------------------------------------------------------------
*/

static int32_t allreduce_pred(int32_t orig_val, void * my_val) {
  if (orig_val == 0) {
    return 0;
  } else {
    return orig_val += 1;
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: 
 --------------------------------------------------------------------
*/

static void gate_allreduce_worker(void * ptr) { /*  */
  uint64_t gen = (uint64_t) ptr;

  /* synchronize the gate with this generation */
  hpx_lco_gate_sync(gate, gen);

  /* you can let the reduction predicate handle the work here, or you can use */
  /* hpx_lco_mutex_lock() and hpx_lco_mutex_unlock() from <hpx/mutex.h> with  */
  /* your own data */

  hpx_lco_gate_set(gate);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: run allreduce on a gate
 --------------------------------------------------------------------
*/

static void run_gate_allreduce(hpx_context_t * ctx) {
  hpx_thread_t *ths[8] = {NULL};
  uint64_t gen = 0;
  uint64_t idx = 0;

  /* get a future associated with the current generation on the gate */
  fut = hpx_lco_gate_get_future(gate, 8, &gen);
  ck_assert_msg(fut != NULL, "Future was NULL.");

  /* create some threads */
  for (idx = 0; idx < 8; idx++) {
    hpx_thread_create(ctx, 0, gate_allreduce_worker, (void *) gen, NULL, NULL);
  }

  /* wait for this generation's future to be triggered */
  hpx_thread_wait(fut);
}


/*
 --------------------------------------------------------------------
  TEST: allreduce on a versioned AND gate
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_gate_allreduce)
{
  hpx_context_t * ctx;
  hpx_config_t cfg;
  uint64_t idx;
  union {
    uint32_t word;
    uint8_t bytes[sizeof(void*)];               /* need at least a word */
  } mydata = { .word = 73 };

  /* initialize a configuration */
  hpx_config_init(&cfg);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create a gate */
  gate = hpx_lco_gate_create(HPX_LCO_GATE_TYPE_AND,
                             HPX_LCO_GATE_MODE_SPARSE | HPX_LCO_GATE_DATATYPE_I32,
                             (__hpx_gate_pred_t)&allreduce_pred, mydata.bytes);
  ck_assert_msg(gate != NULL, "Gate was NULL.");

  /* run allreduce 73 times */
  for (idx = 0; idx < 73; idx++) {
    run_gate_allreduce(ctx);
  }

  /* clean up */
  hpx_lco_gate_destroy(gate);
  hpx_ctx_destroy(ctx);
}
END_TEST


/*
  --------------------------------------------------------------------
  register tests from this file
  --------------------------------------------------------------------
*/

void add_12_gate(TCase *tc) {
  tcase_add_test(tc, test_libhpx_gate_allreduce);
}

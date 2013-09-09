
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

#include <check.h>
#include "hpx/gate.h"

/* Globals */
hpx_gate_t * gate;


/*
 --------------------------------------------------------------------
  TEST HELPER: 
 --------------------------------------------------------------------
*/

void gate_allreduce_worker(void * ptr) {
  uint64_t gen = (uint64_t) ptr;

  /* synchronize the gate with this generation */
  hpx_lco_gate_sync(gate, gen);

  hpx_lco_gate_set(gate);
}


/*
 --------------------------------------------------------------------
  TEST HELPER: run allreduce on a gate
 --------------------------------------------------------------------
*/

void run_gate_allreduce(hpx_context_t * ctx) {
  hpx_thread_t * ths[1024];
  hpx_future_t * fut = NULL;
  uint64_t gen;
  uint64_t idx;

  /* get a future associated with the current generation on the gate */
  fut = hpx_lco_gate_get_future(gate, 1024, &gen);
  ck_assert_msg(fut != NULL, "Future was NULL.");

  /* create some threads */
  for (idx = 0; idx < 1024; idx++) {
    ths[idx] = hpx_thread_create(ctx, 0, (void *) gate_allreduce_worker, (void *) gen);
  }

  hpx_thread_wait(fut);

  /* wait for threads to finish */
  for (idx = 0; idx < 1024; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }
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
  hpx_thread_t * ths[1024];
  uint64_t idx;

  /* initialize a configuration */
  hpx_config_init(&cfg);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create a gate */
  gate = hpx_lco_gate_create(HPX_LCO_GATE_TYPE_AND, NULL);
  ck_assert_msg(gate != NULL, "Gate was NULL.");

  /* run allreduce 73 times */
  for (idx = 0; idx < 73; idx++) {
    run_gate_allreduce(ctx);
  }

  //  /* create some worker threads */
  //  for (idx = 0; idx < 1024; idx++) {
  //    ths[idx] = hpx_thread_create(ctx, 0, (void *) run_gate_test, (void *) idx);
  //  }
  //
  //  /* wait for threads to terminate */
  //  for (idx = 0; idx < 1024; idx++) {
  //    hpx_thread_join(ths[idx], NULL);
  //  }

  /* clean up */
  hpx_lco_gate_destroy(gate);
  hpx_ctx_destroy(ctx);
}
END_TEST



/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Thread Performance Tests - Context Switching
  98_thread_perf1.c

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


#include <time.h>
#include <papi.h>
#include "hpx_thread.h"

/* from autoconf */
#include "config.h"

/* include a header for whatever timing functions we're using */
#ifdef WITH_PAPI
  #include <papi.h>
#else
  #ifdef __linux__
    #include <time.h>
  #endif
#endif


typedef struct {
  uint64_t iters;
  int delay;
} hpx_thread_perf_t;


/*
 --------------------------------------------------------------------
  UTILITY: get a monotonic timestamp with nanosecond resolution
 --------------------------------------------------------------------
*/

#ifdef WITH_PAPI
#define get_ns() PAPI_get_real_nsec()
#else
long long get_ns(void) {
  long long ns = 0;

  #ifdef __linux__
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  ns = (ts.tv_sec * 1000000000) + ts.tv_nsec;  
  #endif

  return ns;
}
#endif

/*
 --------------------------------------------------------------------
  TEST HELPER: baseline timings
 --------------------------------------------------------------------
*/

void loop_function(void * ptr){
  hpx_thread_perf_t * perf = (hpx_thread_perf_t *) ptr;
  uint64_t i;

  volatile double bigcount = 1;
  for(i = 0; i < perf->iters; i++){
    bigcount = bigcount + perf->iters / bigcount;
    if(i % perf->delay == 0)
      bigcount += 3;
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: yield timings
 --------------------------------------------------------------------
*/  

void loop_function2(void * ptr) {
  hpx_thread_perf_t * perf = (hpx_thread_perf_t *) ptr;
  uint64_t i;

  volatile double bigcount = 1;
  for(i = 0; i < perf->iters; i++){
    bigcount = bigcount + perf->iters / bigcount;
    if(i % perf->delay == 0){
      hpx_thread_yield();
    }
  }
}


/*
 --------------------------------------------------------------------
  TEST HELPER: context switch timer
 --------------------------------------------------------------------
*/

void run_yield_timings(uint64_t mflags, uint32_t core_cnt, uint64_t th_cnt, uint64_t iters, int delay) {
  hpx_context_t * ctx = NULL;
  hpx_thread_perf_t perf;
  hpx_config_t cfg;
  uint64_t idx;
  double elapsed1 = 1;
  double elapsed2 = 1;
  void * retval;

  hpx_thread_t * ths[th_cnt];

  long long begin_ts;
  long long end_ts;

#ifdef WITH_PAPI
  PAPI_library_init(PAPI_VER_CURRENT);
#endif

  perf.iters = iters;
  perf.delay = delay;

  /* get a thread context */
  hpx_config_init(&cfg);
  hpx_config_set_switch_flags(&cfg, mflags);

  if (core_cnt > 0) {
    hpx_config_set_cores(&cfg, core_cnt);
  }

  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not create a thread context.");

  /* run the baseline once before measuring, to limit cache misses */
  for (idx = 0; idx < th_cnt; idx++) {
    ths[idx] = hpx_thread_create(ctx, loop_function, &perf);
  }

  /* wait for the threads to finish */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], &retval);
  }

  begin_ts = get_ns();

  /* create threads for the baseline measurement */
  for (idx = 0; idx < th_cnt; idx++) {
    ths[idx] = (hpx_thread_t *) hpx_thread_create(ctx, loop_function, &perf);
  }  

  /* wait for the baseline threads to complete */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join((hpx_thread_t *) ths[idx], &retval);
  }

  end_ts = get_ns();
  elapsed1 = end_ts - begin_ts;

  /* run the yielding threads once to get data locality */
  for (idx = 0; idx < th_cnt; idx++) {
    ths[idx] = hpx_thread_create(ctx, loop_function2, &perf);
  }

  /* wait for them to finish */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], &retval);
  }

  begin_ts = get_ns();

  /* create threads for the yield measurement */
  for (idx = 0; idx < th_cnt; idx++) {
    ths[idx] = hpx_thread_create(ctx, loop_function2, &perf);
  }  

  /* wait for the baseline threads to complete */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], &retval);
  }

  end_ts = get_ns();
  elapsed2 = end_ts - begin_ts;

  printf("  Time spent context switching:                %.4f seconds\n", ((elapsed2 - elapsed1) / 1000000000.0));
  printf("  Percentage of time spent context switching:  %.2f%%\n", (1 - elapsed1 / ((double) (elapsed2))) * 100);
  printf("  Mean context switch time:                    %.2f nanoseconds\n", (elapsed2 - elapsed1) / (th_cnt * (double) iters) * delay);

  hpx_ctx_destroy(ctx);
  ctx = NULL;
}


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch\n");
  printf("250,000 context switches with one HPX thread on one core, no switching flags\n");
  printf("----------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 1, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_ext\n");
  printf("250,000 context switches with one HPX thread on one core, saving FPU state\n");
  printf("--------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 1, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_sig\n");
  printf("250,000 context switches with one HPX thread on one core, saving the signal mask\n");
  printf("--------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 1, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_ext_sig\n");
  printf("250,000 context switches with one HPX thread on one core, saving FPU state and the signal mask\n");
  printf("----------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 1, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on two
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_2th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_2th\n");
  printf("500,000 context switches with two HPX threads on one core, no switching flags\n");
  printf("-----------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 2, 250000000, 1000);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on two HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_2th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_2th_ext\n");
  printf("500,000 context switches with two HPX threads on one core, saving FPU state\n");
  printf("---------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 2, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on two HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_2th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_2th_sig\n");
  printf("500,000 context switches with two HPX threads on one core, saving the signal mask\n");
  printf("---------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 2, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_2th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_2th_ext_sig\n");
  printf("500,000 context switches with two HPX threads on one core, saving FPU state and the signal mask\n");
  printf("-----------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 2, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on three
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_3th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_3th\n");
  printf("750,000 context switches with three HPX threads on one core, no switching flags\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 3, 250000000, 1000);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on three 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_3th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_3th_ext\n");
  printf("750,000 context switches with three HPX threads on one core, saving FPU state\n");
  printf("-----------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 3, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on three HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_3th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_3th_sig\n");
  printf("750,000 context switches with three HPX threads on one core, saving the signal mask\n");
  printf("-----------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 3, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_3th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_3th_ext_sig\n");
  printf("750,000 context switches with three HPX threads on one core, saving FPU state and the signal mask\n");
  printf("-------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 3, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on four
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_4th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_4th\n");
  printf("1,000,000 context switches with four HPX threads on one core, no switching flags\n");
  printf("------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 4, 250000000, 1000);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on four 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_4th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_4th_ext\n");
  printf("1,000,000 context switches with four HPX threads on one core, saving FPU state\n");
  printf("----------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 4, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on four HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_4th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_4th_sig\n");
  printf("1,000,000 context switches with four HPX threads on one core, saving the signal mask\n");
  printf("----------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 4, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_4th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_4th_ext_sig\n");
  printf("1,000,000 context switches with four HPX threads on one core, saving FPU state and the signal mask\n");
  printf("------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 4, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on six
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_6th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_6th\n");
  printf("1,000,000 context switches with six HPX threads on one core, no switching flags\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 6, 250000000, 1000);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on six 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_6th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_6th_ext\n");
  printf("1,000,000 context switches with six HPX threads on one core, saving FPU state\n");
  printf("-----------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 6, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on six HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_6th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_6th_sig\n");
  printf("1,000,000 context switches with six HPX threads on one core, saving the signal mask\n");
  printf("-----------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 6, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  six HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_6th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_6th_ext_sig\n");
  printf("1,000,000 context switches with six HPX threads on one core, saving FPU state and the signal mask\n");
  printf("-------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 6, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on eight
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_8th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_8th\n");
  printf("1,500,000 context switches with eight HPX threads on one core, no switching flags\n");
  printf("---------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 8, 250000000, 1000);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on eight 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_8th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_8th_ext\n");
  printf("1,500,000 context switches with eight HPX threads on one core, saving FPU state\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 8, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on eight HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_8th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_8th_sig\n");
  printf("1,000,000 context switches with eight HPX threads on one core, saving the signal mask\n");
  printf("-------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 8, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  eight HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch_8th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch_8th_ext_sig\n");
  printf("1,000,000 context switches with eight HPX threads on one core, saving FPU state and the signal mask\n");
  printf("---------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 8, 250000000, 1000);  
  printf("\n\n");
}
END_TEST


/* === */


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2\n");
  printf("2,500,000 context switches with one HPX thread on one core, no switching flags\n");
  printf("------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 1, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving extended state
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_ext\n");
  printf("2,500,000 context switches with one HPX thread on one core, saving FPU state\n");
  printf("----------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 1, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_sig\n");
  printf("2,500,000 context switches with one HPX thread on one core, saving the signal mask\n");
  printf("----------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 1, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_ext_sig\n");
  printf("2,500,000 context switches with one HPX thread on one core, saving FPU state and the signal mask\n");
  printf("------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 1, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on two
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_2th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_2th\n");
  printf("5,000,000 context switches with two HPX threads on one core, no switching flags\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 2, 250000000, 100);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on two HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_2th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_2th_ext\n");
  printf("5,000,000 context switches with two HPX threads on one core, saving FPU state\n");
  printf("-----------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 2, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on two HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_2th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_2th_sig\n");
  printf("5,000,000 context switches with two HPX threads on one core, saving the signal mask\n");
  printf("-----------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 2, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_2th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_2th_ext_sig\n");
  printf("5,000,000 context switches with two HPX threads on one core, saving FPU state and the signal mask\n");
  printf("-------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 2, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on three
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_3th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_3th\n");
  printf("7,500,000 context switches with three HPX threads on one core, no switching flags\n");
  printf("---------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 3, 250000000, 100);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on three 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_3th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_3th_ext\n");
  printf("7,500,000 context switches with three HPX threads on one core, saving FPU state\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 3, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on three HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_3th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_3th_sig\n");
  printf("7,500,000 context switches with three HPX threads on one core, saving the signal mask\n");
  printf("-------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 3, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_3th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_3th_ext_sig\n");
  printf("7,500,000 context switches with three HPX threads on one core, saving FPU state and the signal mask\n");
  printf("---------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 3, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on four
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_4th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_4th\n");
  printf("10,000,000 context switches with four HPX threads on one core, no switching flags\n");
  printf("---------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 4, 250000000, 100);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on four 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_4th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_4th_ext\n");
  printf("10,000,000 context switches with four HPX threads on one core, saving FPU state\n");
  printf("-------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 4, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on four HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_4th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_4th_sig\n");
  printf("10,000,000 context switches with four HPX threads on one core, saving the signal mask\n");
  printf("-------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 4, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  two HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_4th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_4th_ext_sig\n");
  printf("10,000,000 context switches with four HPX threads on one core, saving FPU state and the signal mask\n");
  printf("---------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 4, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on six
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_6th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_6th\n");
  printf("15,000,000 context switches with six HPX threads on one core, no switching flags\n");
  printf("--------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 6, 250000000, 100);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on six 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_6th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_6th_ext\n");
  printf("15,000,000 context switches with six HPX threads on one core, saving FPU state\n");
  printf("------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 6, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on six HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_6th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_6th_sig\n");
  printf("15,000,000 context switches with six HPX threads on one core, saving the signal mask\n");
  printf("------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 6, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  six HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_6th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_6th_ext_sig\n");
  printf("15,000,000 context switches with six HPX threads on one core, saving FPU state and the signal mask\n");
  printf("--------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 6, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, no switching flags on eight
  HPX threads on one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_8th)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_8th\n");
  printf("20,000,000 context switches with eight HPX threads on one core, no switching flags\n");
  printf("----------------------------------------------------------------------------------\n"); 
  run_yield_timings(0, 1, 8, 250000000, 100);  
  printf("\n\n");
}
END_TEST



/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU state on eight 
  HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_8th_ext)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_8th_ext\n");
  printf("20,000,000 context switches with eight HPX threads on one core, saving FPU state\n");
  printf("--------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED, 1, 8, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving signals on eight HPX
  threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_8th_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_8th_sig\n");
  printf("20,000,000 context switches with eight HPX threads on one core, saving the signal mask\n");
  printf("--------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_SIGNALS, 1, 8, 250000000, 100);  
  printf("\n\n");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread context switch timings, saving FPU and signals with
  eight HPX threads, one core
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_thread_perf_switch2_8th_ext_sig)
{
  printf("RUNNING PERFORMANCE TEST: test_libhpx_thread_perf_switch2_8th_ext_sig\n");
  printf("20,000,000 context switches with eight HPX threads on one core, saving FPU state and the signal mask\n");
  printf("----------------------------------------------------------------------------------------------------\n"); 
  run_yield_timings(HPX_MCTX_SWITCH_EXTENDED | HPX_MCTX_SWITCH_SIGNALS, 1, 8, 250000000, 100);  
  printf("\n\n");
}
END_TEST


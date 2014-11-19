//****************************************************************************
// @Filename      00_hpxtest.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness
//
// @Compiler      GCC
// @OS            Linux
// @Description   Main function.
// @Goal          Goal of this testcase is to initialize check as HPX
//                application
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/07/2014
// @Author        Patrick K. Bohan <pbohan [at] indiana.edu>
//                Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <check.h>

#include <libhpx/debug.h>
#include <hpx/hpx.h>

#include "tests.h"

//****************************************************************************
//  Globals
//****************************************************************************
hpx_action_t _main = 0;

hpx_action_t t02_init_sources;
hpx_action_t t03_initDomain;
hpx_action_t t04_send;
hpx_action_t t04_sendData;
hpx_action_t t04_recv;
hpx_action_t t04_getContValue;
hpx_action_t t05_initData;
hpx_action_t t05_worker;
hpx_action_t t05_assignID;
hpx_action_t t05_cont_thread;
hpx_action_t t05_thread_cont_cleanup;
hpx_action_t t05_thread_current_cont_target;
hpx_action_t t05_yield_worker;
hpx_action_t t06_set_value;
hpx_action_t t06_get_value;
hpx_action_t t06_get_future_value;
hpx_action_t t06_waitforempty;
hpx_action_t t06_waitforfull;
hpx_action_t t06_getat;
hpx_action_t t06_waitforempty_id;
hpx_action_t t06_waitforfull_id;
hpx_action_t t06_getat_id;
hpx_action_t t06_lcoSetGet;
hpx_action_t t06_set;
hpx_action_t t07_init_array;
hpx_action_t t07_lcoSetGet;
hpx_action_t t07_initMemory;
hpx_action_t t07_initBlock;
hpx_action_t t07_getAll;
hpx_action_t t07_errorSet;
hpx_action_t t08_handler;
hpx_action_t t09_sender;
hpx_action_t t09_receiver;
hpx_action_t t09_sendInOrder;
hpx_action_t t09_receiveInOrder;
hpx_action_t t09_tryRecvEmpty;
hpx_action_t t09_senderChannel;
hpx_action_t t09_receiverChannel;
hpx_action_t t10_set;
hpx_action_t t11_increment;
hpx_action_t t12_init_array;
hpx_action_t t13_memput_verify;

//****************************************************************************
// Options
//****************************************************************************
static void usage(FILE *f) {
  fprintf(f, "Usage: hpxtext [options] ROUNDS \n"
          "\t-s, set timeout for the test-suite\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}

static unsigned _timeout = 1200;

//****************************************************************************
// Main action to run check as HPX Application
//****************************************************************************
Suite *test_suite(void)
{
  Suite * s = suite_create("hpxtest");
  TCase * tc = tcase_create("hpxtest-Core");

  /* install fixtures */
  tcase_add_unchecked_fixture(tc, hpxtest_core_setup, hpxtest_core_teardown);

  /* set timeout */
  if (_timeout)
    tcase_set_timeout(tc, _timeout);

  add_02_TestMemAlloc(tc);
  add_03_TestGlobalMemAlloc(tc);
  add_04_TestParcel(tc);
  add_05_TestThreads(tc);
  add_06_TestFutures(tc);
  //add_06_TestNewFutures(tc);
  add_07_TestLCO(tc);
  add_08_TestSemaphores(tc);
  add_09_TestChannels(tc);
  add_10_TestAndLCO(tc);
  add_11_TestGenCountLCO(tc);
  add_12_TestMemget(tc);
  add_13_TestMemput(tc);

  suite_add_tcase(s, tc);
  return s;
}

int _main_action(void *args)
{
  Suite *s;
  s = test_suite();
  SRunner * sr = srunner_create(s);
  srunner_add_suite(sr, s);

  //Outputs the result to output.log
  srunner_set_log(sr, "output.log");
  // This sets CK_FORK=no
  srunner_set_fork_status(sr, CK_NOFORK);

  srunner_run_all(sr, CK_VERBOSE);
  int failed = srunner_ntests_failed(sr);
  srunner_free(sr);

  return (failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

//****************************************************************************
// Registers functions as actions.
//****************************************************************************
void _register_actions(void) {
  HPX_REGISTER_ACTION(&_main, _main_action);

  // 02_TestMemAlloc.c
  HPX_REGISTER_ACTION(&t02_init_sources, t02_init_sources_action);

  // 03_TestGlobalMemAlloc.c
  HPX_REGISTER_ACTION(&t03_initDomain, t03_initDomain_action);

  //04_TestParcel.c
  HPX_REGISTER_ACTION(&t04_send, t04_send_action);
  HPX_REGISTER_ACTION(&t04_sendData, t04_sendData_action);
  HPX_REGISTER_ACTION(&t04_recv, t04_recv_action);
  HPX_REGISTER_ACTION(&t04_getContValue, t04_getContValue_action);

  //05_TestThreads.c
  HPX_REGISTER_ACTION(&t05_initData, t05_initData_action);
  HPX_REGISTER_ACTION(&t05_worker, t05_worker_action);
  HPX_REGISTER_ACTION(&t05_assignID, t05_assignID_action);
  HPX_REGISTER_ACTION(&t05_cont_thread, t05_set_cont_action);
  HPX_REGISTER_ACTION(&t05_thread_cont_cleanup, t05_thread_cont_cleanup_action);
  HPX_REGISTER_ACTION(&t05_thread_current_cont_target, t05_thread_current_cont_target_action);
  HPX_REGISTER_ACTION(&t05_yield_worker, t05_yield_worker_action);

  //06_TestFutures.c
  HPX_REGISTER_ACTION(&t06_set_value, t06_set_value_action);
  HPX_REGISTER_ACTION(&t06_get_value, t06_get_value_action);
  HPX_REGISTER_ACTION(&t06_get_future_value, t06_get_future_value_action);

  //06_TestNewFutures.c
  /* HPX_REGISTER_ACTION(&t06_waitforempty, t06_waitforempty_action); */
  /* HPX_REGISTER_ACTION(&t06_waitforfull, t06_waitforfull_action); */
  /* HPX_REGISTER_ACTION(&t06_getat, t06_getat_action); */
  /* HPX_REGISTER_ACTION(&t06_waitforempty_id, t06_waitforempty_id_action); */
  /* HPX_REGISTER_ACTION(&t06_waitforfull_id, t06_waitforfull_id_action); */
  /* HPX_REGISTER_ACTION(&t06_getat_id, t06_getat_id_action); */
  /* HPX_REGISTER_ACTION(&t06_lcoSetGet, t06_lcoSetGet_action); */
  /* HPX_REGISTER_ACTION(&t06_set, t06_set_action); */

  //07_TestLCO.c
  HPX_REGISTER_ACTION(&t07_init_array, t07_init_array_action);
  HPX_REGISTER_ACTION(&t07_lcoSetGet, t07_lcoSetGet_action);
  HPX_REGISTER_ACTION(&t07_initMemory, t07_initMemory_action);
  HPX_REGISTER_ACTION(&t07_initBlock, t07_initBlock_action);
  HPX_REGISTER_ACTION(&t07_getAll, t07_getAll_action);
  HPX_REGISTER_ACTION(&t07_errorSet, t07_errorSet_action);

  //08_TestSema.c
  HPX_REGISTER_ACTION(&t08_handler, t08_handler_action);

  //09_TestChannels.c
  HPX_REGISTER_ACTION(&t09_sender, t09_sender_action);
  HPX_REGISTER_ACTION(&t09_receiver, t09_receiver_action);
  HPX_REGISTER_ACTION(&t09_sendInOrder, t09_sendInOrder_action);
  HPX_REGISTER_ACTION(&t09_receiveInOrder, t09_receiveInOrder_action);
  HPX_REGISTER_ACTION(&t09_tryRecvEmpty, t09_tryRecvEmpty_action);
  HPX_REGISTER_ACTION(&t09_senderChannel, t09_senderChannel_action);
  HPX_REGISTER_ACTION(&t09_receiverChannel, t09_receiverChannel_action);

  //10_TestAndLCO.c
  HPX_REGISTER_ACTION(&t10_set, t10_set_action);

  //11_TestGenCount.c
  HPX_REGISTER_ACTION(&t11_increment, t11_increment_action);

  //12_TestMemget.c
  HPX_REGISTER_ACTION(&t12_init_array, t12_init_array_action);

  //13_TestMemput.c
  HPX_REGISTER_ACTION(&t13_memput_verify, t13_memput_verify_action);
}

//****************************************************************************
// Initialize the check and run as a HPX application
//****************************************************************************
int main(int argc, char * argv[]) {

  // Initialize HPX
  hpx_init(&argc, &argv);

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "s:h?")) != -1) {
    switch (opt) {
     case 's':
      _timeout = atoi(optarg);
      break;
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  // Register all the actions
  _register_actions();

  // Run HPX
  return hpx_run(&_main, NULL, 0);
}

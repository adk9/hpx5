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
  fprintf(f, "Usage: CHECK [options] ROUNDS \n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-s, set timeout (0 disables)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, logging level\n"
          "\t-h, show help\n");
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
  _main = HPX_REGISTER_ACTION(_main_action);

  // 02_TestMemAlloc.c
  t02_init_sources = HPX_REGISTER_ACTION(t02_init_sources_action);

  // 03_TestGlobalMemAlloc.c
  t03_initDomain = HPX_REGISTER_ACTION(t03_initDomain_action);

  //04_TestParcel.c
  t04_send = HPX_REGISTER_ACTION(t04_send_action);
  t04_sendData = HPX_REGISTER_ACTION(t04_sendData_action);
  t04_recv = HPX_REGISTER_ACTION(t04_recv_action);
  t04_getContValue = HPX_REGISTER_ACTION(t04_getContValue_action);

  //05_TestThreads.c
  t05_initData = HPX_REGISTER_ACTION(t05_initData_action);
  t05_worker   = HPX_REGISTER_ACTION(t05_worker_action);
  t05_assignID = HPX_REGISTER_ACTION(t05_assignID_action);
  t05_cont_thread = HPX_REGISTER_ACTION(t05_set_cont_action);
  t05_thread_cont_cleanup = HPX_REGISTER_ACTION(t05_thread_cont_cleanup_action);
  t05_thread_current_cont_target = HPX_REGISTER_ACTION(t05_thread_current_cont_target_action);
  t05_yield_worker = HPX_REGISTER_ACTION(t05_yield_worker_action);

  //06_TestFutures.c
  t06_set_value = HPX_REGISTER_ACTION(t06_set_value_action);
  t06_get_value = HPX_REGISTER_ACTION(t06_get_value_action);
  t06_get_future_value = HPX_REGISTER_ACTION(t06_get_future_value_action);

  //06_TestNewFutures.c
  /* t06_waitforempty = HPX_REGISTER_ACTION(t06_waitforempty_action); */
  /* t06_waitforfull = HPX_REGISTER_ACTION(t06_waitforfull_action); */
  /* t06_getat = HPX_REGISTER_ACTION(t06_getat_action); */
  /* t06_waitforempty_id = HPX_REGISTER_ACTION(t06_waitforempty_id_action); */
  /* t06_waitforfull_id = HPX_REGISTER_ACTION(t06_waitforfull_id_action); */
  /* t06_getat_id = HPX_REGISTER_ACTION(t06_getat_id_action); */
  /* t06_lcoSetGet = HPX_REGISTER_ACTION(t06_lcoSetGet_action); */
  /* t06_set = HPX_REGISTER_ACTION(t06_set_action); */

  //07_TestLCO.c
  t07_init_array = HPX_REGISTER_ACTION(t07_init_array_action);
  t07_lcoSetGet = HPX_REGISTER_ACTION(t07_lcoSetGet_action);
  t07_initMemory = HPX_REGISTER_ACTION(t07_initMemory_action);
  t07_initBlock = HPX_REGISTER_ACTION(t07_initBlock_action);
  t07_getAll = HPX_REGISTER_ACTION(t07_getAll_action);
  t07_errorSet = HPX_REGISTER_ACTION(t07_errorSet_action);

  //08_TestSema.c
  t08_handler = HPX_REGISTER_ACTION(t08_handler_action);

  //09_TestChannels.c
  t09_sender = HPX_REGISTER_ACTION(t09_sender_action);
  t09_receiver = HPX_REGISTER_ACTION(t09_receiver_action);
  t09_sendInOrder = HPX_REGISTER_ACTION(t09_sendInOrder_action);
  t09_receiveInOrder = HPX_REGISTER_ACTION(t09_receiveInOrder_action);
  t09_tryRecvEmpty = HPX_REGISTER_ACTION(t09_tryRecvEmpty_action);
  t09_senderChannel = HPX_REGISTER_ACTION(t09_senderChannel_action);
  t09_receiverChannel = HPX_REGISTER_ACTION(t09_receiverChannel_action);

  //10_TestAndLCO.c
  t10_set = HPX_REGISTER_ACTION(t10_set_action);

  //11_TestGenCount.c
  t11_increment = HPX_REGISTER_ACTION(t11_increment_action);

  //12_TestMemget.c
  t12_init_array = HPX_REGISTER_ACTION(t12_init_array_action);

  //13_TestMemput.c
  t13_memput_verify = HPX_REGISTER_ACTION(t13_memput_verify_action);
}

//****************************************************************************
// Initialize the check and run as a HPX application
//****************************************************************************
int main(int argc, char * argv[]) {
  //dbg_wait();

  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:T:s:l:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
      break;
     case 's':
      _timeout = atoi(optarg);
      break;
     case 'l':
      cfg.log_level = atoi(optarg);
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

  // Initialize HPX
  hpx_init(&cfg);

  // Register all the actions
  _register_actions();

  // Run HPX
  return hpx_run(_main, NULL, 0);
}

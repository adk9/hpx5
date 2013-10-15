
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Threads (Stage 2)
  08_thread2.c

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

#include <string.h>
#include "hpx.h"
#include "tests.h"


/*
 --------------------------------------------------------------------
  TEST DATA
 --------------------------------------------------------------------
*/


/*
 --------------------------------------------------------------------
  TEST: CPU core count
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_cores)
{
  hpx_config_t cfg;
  char msg[128];

  hpx_config_init(&cfg);

  /* make sure we have a reasonable default */
  sprintf(msg, "Number of cores was not initialized (expected %d, got %d).", (int) hpx_kthread_get_cores(), hpx_config_get_cores(&cfg));
  ck_assert_msg(hpx_config_get_cores(&cfg) == hpx_kthread_get_cores(), msg);

  /* see if a value gets set */
  hpx_config_set_cores(&cfg, 64);
  sprintf(msg, "Number of cores was not set (expected 64, got %d).", hpx_config_get_cores(&cfg));
  ck_assert_msg(hpx_config_get_cores(&cfg) == 64, msg);

  /* try to set zero cores */
  hpx_config_set_cores(&cfg, 0);
  ck_assert_msg(hpx_config_get_cores(&cfg) == 64, "Number of cores must be at least 1 (expected 64, got 0).");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: FPU switching flag
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_switch_fpu)
{
  hpx_config_t cfg;
  uint8_t flag;
  
  hpx_config_init(&cfg);

  /* FPU switching is disabled by default */
  ck_assert_msg(!(hpx_config_get_switch_flags(&cfg) & HPX_MCTX_SWITCH_EXTENDED), "FPU switching was set by default (it should not have been).");

  /* set the flag */
  hpx_config_set_switch_flags(&cfg, (hpx_config_get_switch_flags(&cfg) | HPX_MCTX_SWITCH_EXTENDED));
  ck_assert_msg(hpx_config_get_switch_flags(&cfg) & HPX_MCTX_SWITCH_EXTENDED, "FPU switching flag was not set.");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: blocked signals mask switching flag
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_switch_sigmask)
{
  hpx_config_t cfg;
  uint8_t flag;
  char msg[128];  

  hpx_config_init(&cfg);

  /* make sure the default is right */
  ck_assert_msg((hpx_config_get_switch_flags(&cfg) & HPX_MCTX_SWITCH_SIGNALS) == 0, "Signal mask switching flag was not initialized.");

  /* set the flag */
  hpx_config_set_switch_flags(&cfg, (hpx_config_get_switch_flags(&cfg) | HPX_MCTX_SWITCH_SIGNALS));
  ck_assert_msg(hpx_config_get_switch_flags(&cfg) & HPX_MCTX_SWITCH_SIGNALS, "Signal mask switching flag was not set.");
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: thread stack size
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_thread_stack_size)
{
  hpx_config_t cfg;
  uint32_t ss;
  char msg[128];

  hpx_config_init(&cfg);
  
  /* make sure the default is right */
  sprintf(msg, "Default thread stack size was not initialized (expected %d, got %d).", HPX_CONFIG_DEFAULT_THREAD_SS, hpx_config_get_thread_stack_size(&cfg));
  ck_assert_msg(hpx_config_get_thread_stack_size(&cfg) == HPX_CONFIG_DEFAULT_THREAD_SS, msg);
  
  /* set the stack size and verify */
  hpx_config_set_thread_stack_size(&cfg, 32768);
  sprintf(msg, "Thread stack size was not set (expected 32768, got %d).", hpx_config_get_thread_stack_size(&cfg));
  ck_assert_msg(hpx_config_get_thread_stack_size(&cfg) == 32768, msg);
}
END_TEST


/*
  --------------------------------------------------------------------
  register tests from this file
  --------------------------------------------------------------------
*/

void add_09_config(TCase *tc) {
  tcase_add_test(tc, test_libhpx_config_cores);
  tcase_add_test(tc, test_libhpx_config_switch_fpu);
  tcase_add_test(tc, test_libhpx_config_switch_sigmask);
  tcase_add_test(tc, test_libhpx_config_thread_stack_size);
}

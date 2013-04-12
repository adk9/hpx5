
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
#include "hpx_thread.h"
#include "hpx_config.h"


/*
 --------------------------------------------------------------------
  TEST DATA
 --------------------------------------------------------------------
*/


/*
 --------------------------------------------------------------------
  TEST: config initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_init)
{
  hpx_config_t cfg;

  memset(&cfg, 0xFF, sizeof(hpx_config_t));

  hpx_config_init(&cfg);

  /* make sure we have reasonable default values */
  ck_assert_msg(hpx_config_get_cores(&cfg) != 0xFFFFFFFF, "Number of cores was not initialized in configuration.");
  ck_assert_msg(hpx_config_get_cores(&cfg) != 0, "Number of cores in configuration can not be zero.");   
}
END_TEST


/*
 --------------------------------------------------------------------
  TEST: get cores
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_config_get_cores)
{
  hpx_config_t cfg;
  uint32_t cores;
  char msg[128];

  hpx_config_init(&cfg);

  cores = hpx_kthread_get_cores();
  sprintf(msg, "Number of cores was not initialized in configuration (expected %d, got %d).");
  ck_assert_msg(hpx_config_get_cores(&cfg) == cores, msg);
}
END_TEST



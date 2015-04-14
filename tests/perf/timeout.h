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
#ifndef LIBHPX_TESTS_PERF_TIMEOUT_H
#define LIBHPX_TESTS_PERF_TIMEOUT_H

#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <hpx/hpx.h>

static int TIMEOUT = 30;

static void timeout(int signal) {
  fprintf(stderr, "test timed out after %d seconds\n", TIMEOUT);
  hpx_shutdown(EXIT_FAILURE);
}

static void set_timeout(int seconds) {
  TIMEOUT = seconds;
  signal(SIGALRM, timeout);
  alarm(TIMEOUT);
}

#endif

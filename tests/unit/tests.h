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
#ifndef LIBHPX_TESTS_H_
#define LIBHPX_TESTS_H_

#include <assert.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hpx/hpx.h>

#define assert_msg(cond, msg) assert(cond && msg)

#define ADD_TEST(test) do {                             \
    int e = hpx_call_sync(HPX_HERE, test, NULL, 0);     \
    assert (e == HPX_SUCCESS);                          \
  } while (0)

// A helper macro to generate a main function template for the test.
#define TEST_MAIN(tests)                                \
  static int _main_handler(void) {                      \
    tests                                               \
    hpx_shutdown(HPX_SUCCESS);                          \
    return HPX_SUCCESS;                                 \
  }                                                     \
  static HPX_ACTION(HPX_DEFAULT, 0, _main,              \
                    _main_handler);                     \
  static void usage(char *prog, FILE *f) {              \
    fprintf(f, "Usage: %s [options]\n"                  \
            "\t-h, show help\n", prog);                 \
    hpx_print_help();                                   \
    fflush(f);                                          \
  }                                                     \
  int main(int argc, char *argv[]) {                    \
    if (hpx_init(&argc, &argv)) {                       \
      fprintf(stderr, "failed to initialize HPX.\n");   \
      return 1;                                         \
    }                                                   \
    int opt = 0;                                        \
    while ((opt = getopt(argc, argv, "h?")) != -1) {    \
      switch (opt) {                                    \
       case 'h':                                        \
        usage(argv[0], stdout);                         \
        return 0;                                       \
       case '?':                                        \
       default:                                         \
        usage(argv[0], stderr);                         \
        return -1;                                      \
      }                                                 \
    }                                                   \
    return hpx_run(&_main);                             \
  }                                                     \
  int main(int argc, char *argv[])



#endif /* LIBHPX_TESTS_H_ */

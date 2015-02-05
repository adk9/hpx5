#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <string.h>
#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <hpx/hpx.h>

#define ADD_TEST(test) do {                                  \
    int e = hpx_call_sync(HPX_HERE, test, NULL, 0, NULL, 0); \
    assert (e == HPX_SUCCESS);                               \
  } while (0)
        

// A helper macro to generate a main function template for the test.
#define TEST_MAIN(tests)                             \
static HPX_ACTION(_main, void *UNUSED) {             \
  tests                                              \
  return HPX_SUCCESS;                                \
}                                                    \
int main(int argc, char *argv[]) {                   \
  if (hpx_init(&argc, &argv)) {                      \
    fprintf(stderr, "failed to initialize HPX.\n");  \
    return 1;                                        \
  }                                                  \
  int _topt = 0;                                     \
  while ((_topt = getopt(argc, argv, "h?")) != -1) { \
    switch (_topt) {                                 \
     case 'h':                                       \
      usage(stdout);                                 \
      return 0;                                      \
     case '?':                                       \
     default:                                        \
      usage(stderr);                                 \
      return -1;                                     \
    }                                                \
  }                                                  \
  return hpx_run(&_main, NULL, 0);                   \
}                                                    \
int main(int argc, char *argv[])                     \


#endif /* LIBHPX_TESTS_TESTS_H_ */

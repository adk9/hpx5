#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <hpx/hpx.h>

#define assert_msg(cond, msg) assert(cond && msg)

#define ADD_TEST(test) do {                                  \
    int e = hpx_call_sync(HPX_HERE, test, NULL, 0, NULL, 0); \
    assert (e == HPX_SUCCESS);                               \
  } while (0)

// A helper macro to generate a main function template for the test.
#define TEST_MAIN(tests)                             \
static HPX_ACTION(_main, void *UNUSED) {             \
  tests                                              \
  hpx_shutdown(HPX_SUCCESS);                         \
  return HPX_SUCCESS;                                \
}                                                    \
int main(int argc, char *argv[]) {                   \
  if (hpx_init(&argc, &argv)) {                      \
    fprintf(stderr, "failed to initialize HPX.\n");  \
    return 1;                                        \
  }                                                  \
  return hpx_run(&_main, NULL, 0);                   \
}                                                    \
int main(int argc, char *argv[])                     \


#endif /* LIBHPX_TESTS_TESTS_H_ */

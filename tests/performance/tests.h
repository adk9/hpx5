#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include <hpx/hpx.h>

void perftest_core_setup(void);
void perftest_core_teardown(void);

void add_primesieve(TCase *);

#endif /* LIBHPX_TESTS_TESTS_H_ */

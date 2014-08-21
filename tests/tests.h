#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestMemMove(TCase *);
void add_05_TestParcel(TCase *);

#endif /* LIBHPX_TESTS_TESTS_H_ */

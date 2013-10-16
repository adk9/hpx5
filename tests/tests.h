#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_mem(TCase *);
void add_03_ctx(TCase *);
void add_04_thread1(TCase *);
void add_05_queue(TCase *);
void add_06_kthread(TCase *);
void add_07_mctx(TCase *, char *long_tests);
void add_08_thread2(TCase *, char *long_tests, char *hardcore_tests);
void add_09_config(TCase *);
void add_10_list(TCase *);
void add_11_map(TCase *);
void add_12_gate(TCase *);
void add_12_parcelhandler(TCase *);
void add_98_thread_perf1(TCase *);

#endif /* LIBHPX_TESTS_TESTS_H_ */

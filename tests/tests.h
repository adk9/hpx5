#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include <hpx/hpx.h>
#include "domain.h"

extern hpx_action_t t02_init_sources;
extern hpx_action_t t03_initDomain;
extern hpx_action_t t04_send;
extern hpx_action_t t05_initData;

int t02_init_sources_action(void*);
int t03_initDomain_action(const InitArgs*);
int t04_send_action(void*);
int t05_initData_action(const InitBuffer*);

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestParcel(TCase *);
void add_05_TestThreads(TCase *);

#endif /* LIBHPX_TESTS_TESTS_H_ */

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
extern hpx_action_t t05_worker;
extern hpx_action_t t05_assignID;
extern hpx_action_t t05_update_array;
extern hpx_action_t t05_init_array;
extern hpx_action_t t05_memput;
extern hpx_action_t t05_memget;
extern hpx_action_t t05_threadContMain;
extern hpx_action_t t06_initDomain;
extern hpx_action_t t06_advanceDomain;

int t02_init_sources_action(void*);
int t03_initDomain_action(const InitArgs*);
int t04_send_action(void*);
int t05_initData_action(const InitBuffer*);
int t05_worker_action(int*);
int t05_assignID_action(void*);
int t05_memput_action(void*);
int t05_memget_action(size_t*);
int t05_init_array_action(contTest_config_t*);
int t05_update_array_action(contTest_config_t*);
int t05_threadContMain_action(contTest_config_t*);
int t06_initDomain_action(const InitArgs*);
int t06_advanceDomain_action(const unsigned long*);

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestParcel(TCase *);
void add_05_TestThreads(TCase *);
void add_06_TestFutures(TCase *);
#endif /* LIBHPX_TESTS_TESTS_H_ */

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
extern hpx_action_t t04_sendData;
extern hpx_action_t t04_recv;
extern hpx_action_t t05_initData;
extern hpx_action_t t05_worker;
extern hpx_action_t t05_assignID;
extern hpx_action_t t05_cont_thread;
extern hpx_action_t t05_thread_cont_cleanup;
extern hpx_action_t t05_thread_current_cont_target;
extern hpx_action_t t05_thread_yield_producer;
extern hpx_action_t t05_thread_yield_consumer;
extern hpx_action_t t06_get_value;
extern hpx_action_t t06_set_value;
extern hpx_action_t t06_get_future_value;

int t02_init_sources_action(void*);
int t03_initDomain_action(const InitArgs*);
int t04_send_action(void*);
int t04_sendData_action(const initBuffer_t*);
int t04_recv_action(double*);
int t05_initData_action(const initBuffer_t*);
int t05_worker_action(int*);
int t05_assignID_action(void*);
int t05_set_cont_action(void*);
int t05_thread_cont_cleanup_action(void*);
int t05_thread_current_cont_target_action(void*);
int t05_thread_yield_producer_action(void*);
int t05_thread_yield_consumer_action(void*);
int t06_set_value_action(void*);
int t06_get_value_action(void*);
int t06_get_future_value_action(void*);

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestParcel(TCase *);
void add_05_TestThreads(TCase *);
void add_06_TestFutures(TCase *);
#endif /* LIBHPX_TESTS_TESTS_H_ */

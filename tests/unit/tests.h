#ifndef LIBHPX_TESTS_TESTS_H_
#define LIBHPX_TESTS_TESTS_H_

#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include <hpx/hpx.h>
#include "domain.h"

FILE * test_log;

extern hpx_action_t t02_init_sources;
extern hpx_action_t t03_initDomain;
extern hpx_action_t t04_send;
extern hpx_action_t t04_sendData;
extern hpx_action_t t04_recv;
extern hpx_action_t t04_getContValue;
extern hpx_action_t t05_initData;
extern hpx_action_t t05_worker;
extern hpx_action_t t05_assignID;
extern hpx_action_t t05_cont_thread;
extern hpx_action_t t05_thread_cont_cleanup;
extern hpx_action_t t05_thread_current_cont_target;
extern hpx_action_t t05_yield_worker;
extern hpx_action_t t06_get_value;
extern hpx_action_t t06_set_value;
extern hpx_action_t t06_get_future_value;
extern hpx_action_t t07_init_array;
extern hpx_action_t t07_lcoSetGet;
extern hpx_action_t t07_initMemory;
extern hpx_action_t t07_initBlock;
extern hpx_action_t t07_getAll;
extern hpx_action_t t07_errorSet;
extern hpx_action_t t08_handler;
extern hpx_action_t t09_sender;
extern hpx_action_t t09_receiver;
extern hpx_action_t t09_sendInOrder;
extern hpx_action_t t09_receiveInOrder;
extern hpx_action_t t09_tryRecvEmpty;
extern hpx_action_t t09_senderChannel;
extern hpx_action_t t09_receiverChannel;
extern hpx_action_t t10_set;
extern hpx_action_t t11_increment;
extern hpx_action_t t12_init_array;
extern hpx_action_t t13_init_array;

int t02_init_sources_action(void*);
int t03_initDomain_action(const InitArgs*);
int t04_send_action(void*);
int t04_sendData_action(const initBuffer_t*);
int t04_recv_action(double*);
int t04_getContValue_action(uint64_t*);
int t05_initData_action(const initBuffer_t*);
int t05_worker_action(uint64_t*);
int t05_assignID_action(void*);
int t05_set_cont_action(void*);
int t05_thread_cont_cleanup_action(void*);
int t05_thread_current_cont_target_action(void*);
int t05_yield_worker_action(void *vargs);
int t06_set_value_action(void*);
int t06_get_value_action(void*);
int t06_get_future_value_action(void*);
int t07_init_array_action(size_t*);
int t07_lcoSetGet_action(uint64_t*);
int t07_initMemory_action(uint32_t*);
int t07_initBlock_action(uint32_t*);
int t07_getAll_action(uint32_t*);
int t07_errorSet_action(void*);
int t08_handler_action(uint32_t*);
int t09_sender_action(hpx_addr_t*);
int t09_receiver_action(hpx_addr_t*);
int t09_sendInOrder_action(hpx_addr_t*);
int t09_receiveInOrder_action(hpx_addr_t*);
int t09_tryRecvEmpty_action(void*);
int t09_senderChannel_action(hpx_addr_t*);
int t09_receiverChannel_action(hpx_addr_t*);
int t10_set_action(void*);
int t11_increment_action(void*);
int t12_init_array_action(size_t*);
int t13_init_array_action(size_t*);

void hpxtest_core_setup(void);
void hpxtest_core_teardown(void);

void add_02_TestMemAlloc(TCase *);
void add_03_TestGlobalMemAlloc(TCase *);
void add_04_TestParcel(TCase *);
void add_05_TestThreads(TCase *);
void add_06_TestFutures(TCase *);
void add_07_TestLCO(TCase *);
void add_08_TestSemaphores(TCase *);
void add_09_TestChannels(TCase *);
void add_10_TestAndLCO(TCase *);
void add_11_TestGenCountLCO(TCase *);
void add_12_TestMemget(TCase *);
void add_13_TestMemput(TCase *);
#endif /* LIBHPX_TESTS_TESTS_H_ */


/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library Unit Test Harness - Parcel Handler Creation
  12_parchandler.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Benjamin D. Martin <benjmart [at] indiana.edu>
 ====================================================================
*/

#include "hpx.h"
#include "tests.h"

/* steal some internal headers so that we can actually write the test */
#include "libhpx/init.h"
#include "libhpx/parcelhandler.h"
#include "libhpx/parcel.h"
#include "parcel/parcelqueue.h"
#include "parcel/serialization.h"

/* meaningless arguments for parcels */
struct args {
  double x;
  int y;
  char z;
};

struct args data_to_check = {1.414, 3, 'a'};

/* arguments to use for parcel send tests */
struct send_args {
  hpx_future_t *fut;
  unsigned int src_rank;
  char* in_data[];
};

static int DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS =  (10*1037)/sizeof(size_t);

static hpx_future_t sendtest_fut; /* used for the simplest send test */
static hpx_future_t sendtest_data_fut; /* used for the data send test */
static hpx_future_t sendtest_datalarge_fut; /* used for the large data sent test */

/*
 -------------------------------------------------------------------
  TEST HELPER: empty action for action/parcel tests
 -------------------------------------------------------------------
*/

static hpx_action_t empty_action;
static void empty(void* args) {
  return;
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for parcel send test, to set future
 -------------------------------------------------------------------
*/

static hpx_action_t set_sendtest_future_action;
static void set_sendtest_future(void* args) {
  hpx_lco_future_set_state(&sendtest_fut);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for parcel send data test, to set future
 -------------------------------------------------------------------
*/

static hpx_action_t set_sendtest_data_future_action;
static void set_sendtest_data_future(void* args) {
  hpx_lco_future_set_state(&sendtest_data_fut);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for parcel send data test, to set future
 -------------------------------------------------------------------
*/

static hpx_action_t set_sendtest_datalarge_future_action;
static void set_sendtest_datalarge_future(void* args) {
  hpx_lco_future_set_state(&sendtest_datalarge_fut);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for some parcel send tests, to set future
 -------------------------------------------------------------------
*/

static hpx_action_t set_future_action;
static void set_future(void* args) {
  hpx_future_t* fut = (hpx_future_t*)args;
  ck_assert_msg(fut != NULL, "Couldn't set future - no argument received");
  hpx_lco_future_set_state(fut);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: worker for parcelqueue tests
 -------------------------------------------------------------------
*/

static void thread_queue_worker(void* a) {
  parcelqueue_t *q = (parcelqueue_t*)a;
  int i;
  int success;
  hpx_parcel_t* val;
  for (i = 0; i < 7; i++) {
    val = hpx_alloc(sizeof(struct hpx_parcel));
    success = parcelqueue_push(q, val);
    ck_assert_msg(success == 0, "Could not push to parcelqueue");
  }
  //  printf("thread %ld is done\n", hpx_thread_get_id(hpx_thread_self()));
  return;
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for send parcel tests
 -------------------------------------------------------------------
*/

static hpx_action_t checkrank_action;
static void checkrank(void* args) {
  int success;
  unsigned int my_rank;
  hpx_parcel_t *p;
  hpx_locality_t* other_loc;

  my_rank = hpx_get_rank();
  ck_assert_msg(my_rank == 1, "Parcel sent to wrong locality");
  
  other_loc = hpx_locality_from_rank(0);
  p = (hpx_parcel_t*)hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel(set_sendtest_future_action, (void*)NULL, 0, p);   
  ck_assert_msg(success == 0, "Couldn't create return parcel");
  success = hpx_send_parcel(other_loc, p);
  ck_assert_msg(success == 0, "Couldn't send parcel");

  hpx_locality_destroy(other_loc);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for send parcel data tests
 -------------------------------------------------------------------
*/

static hpx_action_t checkdata_action;
static void checkdata(void* args) {
  int success;
  hpx_parcel_t *p;
  hpx_locality_t* other_loc;
  ck_assert_msg(args != NULL, "Did not receive any data");
  struct send_args *p_args = (struct send_args*)args;
  struct args *in_data = (struct args*)p_args->in_data;
  ck_assert_msg(in_data != NULL, "Did not receive data");
  if (in_data != NULL) {
    /*
    printf("=====================================================\n");
    printf("Expected %f, received %f\n", data_to_check.x, in_data->x);
    printf("Expected %d, received %d\n", data_to_check.y, in_data->y);
    printf("Expected %c, received %c\n", data_to_check.z, in_data->z);
    printf("=====================================================\n");
    fflush(stdout);
    */
    ck_assert_msg(in_data->x == data_to_check.x, "Did not receive correct data at beginning");
    ck_assert_msg(in_data->y == data_to_check.y, "Did not receive correct data in middle");
    ck_assert_msg(in_data->z == data_to_check.z, "Did not receive correct data at end");
  }

  other_loc = hpx_locality_from_rank(p_args->src_rank);
  void* return_fut = hpx_alloc(sizeof(void*));
  return_fut = (void*)p_args->fut;
  p = (hpx_parcel_t*)hpx_alloc(sizeof(hpx_parcel_t));
  //  success = hpx_new_parcel("_set_future_action", &return_fut, sizeof(void*), p);   
  success = hpx_new_parcel(set_sendtest_data_future_action, NULL, 0, p);   
  ck_assert_msg(success == 0, "Couldn't create return parcel");
  success = hpx_send_parcel(other_loc, p);
  ck_assert_msg(success == 0, "Couldn't send parcel");

  //  free(in_data);
  free(p_args);
  hpx_locality_destroy(other_loc);
}

/*
 -------------------------------------------------------------------
  TEST HELPER: action for send parcel (large) data tests
 -------------------------------------------------------------------
*/

static hpx_action_t checkdata_large_action;
static void checkdata_large(void* args) {
  int success;
  hpx_parcel_t *p;
  hpx_locality_t* other_loc;
  size_t s_i;

  ck_assert_msg(args != NULL, "Did not receive any data");
  struct send_args *p_args = (struct send_args*)args;
  size_t *data = (size_t*)p_args->in_data;
  ck_assert_msg(data != NULL, "Did not receive data");

  for (s_i = 0; s_i < DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS/sizeof(size_t); s_i++)
    ck_assert_msg(data[s_i] == s_i, "Sent data was corrupt");

  other_loc = hpx_locality_from_rank(p_args->src_rank);
  p = (hpx_parcel_t*)hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel(set_sendtest_datalarge_future_action, (void*)NULL, 0, p);   
  ck_assert_msg(success == 0, "Couldn't create return parcel");
  success = hpx_send_parcel(other_loc, p);
  ck_assert_msg(success == 0, "Couldn't send parcel");

  free(p_args);
  hpx_locality_destroy(other_loc);
}

/*
 --------------------------------------------------------------------
  TEST HELPER: Main thread for parcel send test
 --------------------------------------------------------------------
*/
static void _thread_main_parcelsend(void* args) {
  int success;
  hpx_parcel_t* p;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_locality_t* other_loc;

  num_ranks = hpx_get_num_localities();
  ck_assert_msg(num_ranks > 1, "Couldn't send parcel - no remote localities available to send to");

  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  if (my_rank == 0) {
    hpx_lco_future_init(&sendtest_fut);
    other_loc = hpx_locality_from_rank(1);

    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel(checkrank_action, (void*)NULL, 0, p);  
    success = hpx_send_parcel(other_loc, p);
    ck_assert_msg(success == 0, "Couldn't send parcel");

    hpx_thread_wait(&sendtest_fut);

    hpx_locality_destroy(other_loc);
    hpx_lco_future_destroy(&sendtest_fut);
  }
  else {
  }

}

/*
 --------------------------------------------------------------------
  TEST HELPER: Main thread for parcel send data test
 --------------------------------------------------------------------
*/
static void _thread_main_parcelsenddata(void* args) {
  int success;
  hpx_parcel_t* p;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_locality_t* other_loc;
  hpx_future_t fut;

  num_ranks = hpx_get_num_localities();
  ck_assert_msg(num_ranks > 1, "Couldn't send parcel - no remote localities available to send to");

  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  if (my_rank == 0) {
    hpx_lco_future_init(&sendtest_data_fut);
    hpx_lco_future_init(&fut);
    other_loc = hpx_locality_from_rank(1);

    size_t size_of_sendargs = sizeof(struct send_args) + sizeof(struct args);
    struct send_args* args = hpx_alloc(size_of_sendargs);
    args->fut = &fut;
    args->src_rank = my_rank;
    memcpy(args->in_data, &data_to_check, sizeof(struct args));

    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel(checkdata_action, (char*)args, size_of_sendargs, p);
    success = hpx_send_parcel(other_loc, p);
    ck_assert_msg(success == 0, "Couldn't send parcel");

    //    hpx_thread_wait(&fut);
    hpx_thread_wait(&sendtest_data_fut);

    hpx_locality_destroy(other_loc);
    hpx_lco_future_destroy(&fut);
  }
  else {
  }

  /* cleanup */


}

/*
 --------------------------------------------------------------------
  TEST HELPER: Main thread for parcel send (large) data test
 --------------------------------------------------------------------
*/
static void _thread_main_parcelsenddata_large(void* args) {
  int success;
  hpx_parcel_t* p;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_locality_t* other_loc;
  size_t *data_to_send;
  int i;
  hpx_future_t fut;
  
  num_ranks = hpx_get_num_localities();
  ck_assert_msg(num_ranks > 1, "Couldn't send parcel - no remote localities available to send to");

  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  if (my_rank == 0) {
    hpx_lco_future_init(&sendtest_data_fut);
    other_loc = hpx_locality_from_rank(1);

    data_to_send = hpx_alloc(sizeof(struct send_args) + DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS);
    ck_assert_msg(data_to_send != NULL, "Could not send large data - not enough memory to allocate space for data");
    for (i = 0; i < DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS/sizeof(size_t); i++)
      data_to_send[i] = i;

    size_t size_of_sendargs = sizeof(struct send_args) +  DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS;
    struct send_args* args = hpx_alloc(size_of_sendargs);
    args->fut = NULL; // &fut;
    args->src_rank = my_rank;
    memcpy(args->in_data, data_to_send, DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS);

    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel(checkdata_large_action, (char*)args, sizeof(struct send_args) + DATA_SIZE_FOR_PARCEL_SEND_LARGE_TESTS, p);   
    success = hpx_send_parcel(other_loc, p);
    ck_assert_msg(success == 0, "Couldn't send parcel");



    hpx_thread_wait(&sendtest_datalarge_fut);

    hpx_locality_destroy(other_loc);
    hpx_lco_future_destroy(&sendtest_data_fut);
  }
  else {
  }
}

/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set
 --------------------------------------------------------------------
*/

static void run_multi_thread_set_concurrent(uint32_t th_cnt, hpx_func_t th_func, void* args[]) {
  hpx_context_t * ctx = __hpx_global_ctx;
  hpx_future_t ** futs;
  //  hpx_config_t cfg;
  int idx;

  /* init our config */
  //  hpx_config_init(&cfg);

  /* get a thread context */
  //  ctx = hpx_ctx_create(&cfg);
  //  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create HPX theads */
  futs = hpx_calloc(th_cnt, sizeof(futs[0]));
  ck_assert_msg(futs != NULL, "Could not allocate an array to hold future data.");

  for(idx = 0; idx < th_cnt; idx++) {
    hpx_thread_create(ctx, 0, th_func, args[idx], &futs[idx], NULL);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_wait(futs[idx]);
  }
  
  /* clean up */
  hpx_free(futs);
  //  hpx_ctx_destroy(ctx);
}



/*
 --------------------------------------------------------------------
  TEST: parcel handler queue init
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_create)
{
  bool success;
  parcelqueue_t* q;
  success = parcelqueue_create(&q);
  ck_assert_msg(success == 0, "Could not initialize parcelqueue");
  parcelqueue_destroy(&q);
  
}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler queue push and pop
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_push)
{
  int i;
  int success;
  parcelqueue_t* q;

  success = parcelqueue_create(&q);

  hpx_parcel_t* vals[7];
  for (i = 0; i < 7; i++) {
    vals[i] = hpx_alloc(sizeof(hpx_parcel_t));
    memset(vals[i], 0, sizeof(hpx_parcel_t));
    success = parcelqueue_push(q, vals[i]);
    ck_assert_msg(success == 0, "Could not push to parcelqueue");
  }
  
  hpx_parcel_t* pop_vals[7];
  for (i = 0; i < 7; i++) {
    pop_vals[i] = (hpx_parcel_t*)parcelqueue_trypop(q);
    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
    ck_assert_msg(pop_vals[i] == vals[i], "Popped bad value from parcelqueue");
    hpx_free(pop_vals[i]);
  }
  
  parcelqueue_destroy(&q);

}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler queue push and pop, multithreaded
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_push_multithreaded)
{
  int i, idx;
  int success;
  parcelqueue_t* q;
  int count;
  count = 1000;
  parcelqueue_t* args[count];

  success = parcelqueue_create(&q);

  for (i = 0; i < count; i++)
    args[i] = q;

  run_multi_thread_set_concurrent(count, (hpx_func_t)thread_queue_worker, (void**)args);
  /* queue is full HERE */

  hpx_parcel_t* pop_vals[count];
  for (i = 0; i < count; i++) {
    pop_vals[i] = (hpx_parcel_t*)parcelqueue_trypop(q); /* normally this could be NULL, but we've already filled the queue, so we DON'T spin until non-NULL */
    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
    hpx_free(pop_vals[i]);
  }

  parcelqueue_destroy(&q);

}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler queue push and pop, multithreaded with concurrent pops
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_push_multithreaded_concurrent)
{
  int count;
  count = 1000;

  hpx_context_t * ctx = __hpx_global_ctx;
  hpx_thread_t ** ths;
  //  hpx_config_t cfg;
  int idx;
  int i;
  int success;
  parcelqueue_t* q;
  hpx_future_t** futs;
  hpx_parcel_t* pop_vals[count];
  for (i = 0; i < count; i++) {
    pop_vals[i] = NULL;
  }


  /* init our config */
  //  hpx_config_init(&cfg);

  /* get a thread context */
  //  ctx = hpx_ctx_create(&cfg);
  //  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create parcelqueue */
  success = parcelqueue_create(&q);

  /* initialize arguments */

  /* create HPX theads */
  futs = hpx_calloc(count, sizeof(*futs));
  ck_assert_msg(futs != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < count; idx++) {
    hpx_thread_create(ctx, 0, thread_queue_worker, (void*)q, &futs[idx], NULL);
  }

  /* pop values from main thread BEFORE all other threads are finished */
  for (i = 0; i < count; i++) {
    while (pop_vals[i] == NULL) {
      pop_vals[i] = (hpx_parcel_t*)parcelqueue_trypop(q);
    }
    //    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
  }

  /* wait until our threads are done */
  for (idx = 0; idx < count; idx++) {
    hpx_thread_wait(futs[idx]);
  }
  
  /* clean up */
  for (i = 0; i < count; i++) /* this is normally done right after popping, but we want to stress the queue so we wait */
    hpx_free(pop_vals[i]);
  hpx_free(futs);
  //  hpx_ctx_destroy(ctx);


}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler creation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelhandler_create)
{
  /* if (__hpx_parcelhandler == NULL) { /\* if we've run init, this won't work, and */
  /*                                     * besides we know parcelhandler_create */
  /*                                     * works anyway because it's called by */
  /*                                     * init() *\/  */
  /* LD: why doesn't this work? just allocating a parcelhandler (which we don't
     do anything with, should be fine. */
  struct parcelhandler *ph = NULL;
    
  ph = parcelhandler_create(NULL);
  ck_assert_msg(ph != NULL, "Could not create parcelhandler, TODO: need context");
  if (ph != NULL)
    parcelhandler_destroy(ph);
  ph = NULL;
  
  /* } */
} 
END_TEST

/*
 --------------------------------------------------------------------
  TEST: action registration
 --------------------------------------------------------------------
*/

/*TODO: Move action tests to their own file */
START_TEST (test_libhpx_action_register)
{
  empty_action = hpx_action_register("_empty_action", empty);
  hpx_action_registration_complete();
  ck_assert_msg(true, "TODO: fixme");
} 
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel system initialization
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelsystem_init)
{

  int success;
  success = hpx_parcel_init();
  ck_assert_msg(success == 0, "Could not initialize parcel system");

} 
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel creation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcel_create)
{
  struct args Args;
  hpx_parcel_t* p;
  int success;

  empty_action = hpx_action_register("_empty_action", empty);
  hpx_action_registration_complete();
  
  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel(empty_action, (char*)&Args, sizeof(struct args), p);

  ck_assert_msg(success == 0, "Could not create parcel");
  ck_assert_msg(p->action == empty_action, "Error creating parcel - wrong action");
  
} 
END_TEST

/* TODO: send to own locality to test shortcuts */

/*
 --------------------------------------------------------------------
  TEST: parcel serialize
 --------------------------------------------------------------------
*/
START_TEST (test_libhpx_parcel_serialize)
{
  struct args Args = data_to_check;
  hpx_parcel_t* p;
  int success;
  struct header* blob;

  empty_action = hpx_action_register("_empty_action", empty);

  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel(empty_action, (char*)&Args, sizeof(struct args), p);

  ck_assert_msg(success == 0, "Could not serialize parcel - failed to create parcel");

  success = serialize(p, &blob);
  ck_assert_msg(success == 0, "Could not serialize parcel");
  ck_assert_msg(true, "TODO: fixme");
  
  /* int action_name_matches; */
  /* action_name_matches = strncmp("_empty_action", blob + sizeof(hpx_parcel_t), strlen("_empty_action")); */
  /* ck_assert_msg(action_name_matches == 0, "Parcel not serialized properly - action name was incorrect or missing"); */
  /* int payload_matches; */
  /* payload_matches = memcmp((char*)&Args, (blob) + sizeof(hpx_parcel_t) + strlen("_empty_action") + 1, sizeof(struct args)); */
  /* ck_assert_msg(action_name_matches == 0, "Parcel not serialized properly - action name was incorrect or missing"); */
} 
END_TEST



/*
 --------------------------------------------------------------------
  TEST: parcel send
        This test is designed to be run in a networked environment!
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcel_send)
{
  int success;

  //  hpx_config_t *cfg;
  hpx_context_t *ctx = __hpx_global_ctx;
  hpx_future_t *result;

  /* register action for parcel (must be done by all ranks) */
  checkrank_action = hpx_action_register("_checkrank_action", checkrank);
  set_sendtest_future_action = hpx_action_register("_set_sendtest_future_action",
                                                   set_sendtest_future);
  hpx_action_registration_complete();
  
  //  cfg = hpx_alloc(sizeof(hpx_config_t));  
  //  hpx_config_init(cfg);
  //  ctx = hpx_ctx_create(cfg);
  hpx_thread_create(ctx, 0, (hpx_func_t)_thread_main_parcelsend, 0, &result, NULL);
  hpx_thread_wait(result);

  /* cleanup */
  //  hpx_ctx_destroy(ctx); /* note we don't need to free the context - destroy does that */
  //  hpx_free(cfg);
} 
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel send data
        This test is designed to be run in a networked environment!
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcel_senddata)
{
  int success;

  //  hpx_config_t *cfg;
  hpx_context_t *ctx = __hpx_global_ctx;
  hpx_future_t* f;

  /* register action for parcel (must be done by all ranks) */
  checkdata_action = hpx_action_register("_checkdata_action", checkdata);
  set_future_action = hpx_action_register("_set_future_action", set_future);
  set_sendtest_data_future_action = hpx_action_register("_set_sendtest_data_future_action",
                                                        set_sendtest_data_future);
  hpx_action_registration_complete();

  //  cfg = hpx_alloc(sizeof(hpx_config_t));  
  //  hpx_config_init(cfg);
  //  ctx = hpx_ctx_create(cfg);
  hpx_thread_create(ctx, 0, (hpx_func_t)_thread_main_parcelsenddata, 0, &f, NULL);
  hpx_thread_wait(f);

  /* cleanup */
  //  hpx_ctx_destroy(ctx); /* note we don't need to free the context - destroy does that */
  //  hpx_free(cfg);
} 
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel send (large) data
        This test is designed to be run in a networked environment!
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcel_senddata_large)
{
  int success;

  //  hpx_config_t *cfg;
  hpx_context_t *ctx = __hpx_global_ctx;
  hpx_future_t* f;

  /* register action for parcel (must be done by all ranks) */
  checkdata_large_action = hpx_action_register("_checkdata_large_action",
                                               checkdata_large);
  set_future_action = hpx_action_register("_set_future_action", set_future);
  set_sendtest_datalarge_future_action =
    hpx_action_register("_set_sendtest_datalarge_future_action",
                        set_sendtest_datalarge_future);
  hpx_action_registration_complete();
  
  //  cfg = hpx_alloc(sizeof(hpx_config_t));  
  //  hpx_config_init(cfg);
  //  ctx = hpx_ctx_create(cfg);
  hpx_thread_create(ctx, 0, (hpx_func_t)_thread_main_parcelsenddata_large, 0, &f, NULL);
  hpx_thread_wait(f);

  /* cleanup */
  // hpx_ctx_destroy(ctx); /* note we don't need to free the context - destroy does that */
  //  hpx_free(cfg);
} 
END_TEST


/*
  --------------------------------------------------------------------
  register tests from this file
  --------------------------------------------------------------------
*/


void add_12_parcelhandler(TCase *tc) {
  tcase_add_test(tc, test_libhpx_parcelqueue_create);
  tcase_add_test(tc, test_libhpx_parcelqueue_push);
  tcase_add_test(tc, test_libhpx_parcelqueue_push_multithreaded);
  tcase_add_test(tc, test_libhpx_parcelqueue_push_multithreaded_concurrent);
  tcase_add_test(tc, test_libhpx_parcelhandler_create);
  tcase_add_test(tc, test_libhpx_action_register);
  tcase_add_test(tc, test_libhpx_parcelsystem_init);
  tcase_add_test(tc, test_libhpx_parcel_create);
  tcase_add_test(tc, test_libhpx_parcel_serialize);
  tcase_add_test(tc, test_libhpx_parcel_send);
  tcase_add_test(tc, test_libhpx_parcel_senddata);
  tcase_add_test(tc, test_libhpx_parcel_senddata_large);
}

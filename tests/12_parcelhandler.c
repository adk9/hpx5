
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

#include <check.h>
#include "hpx.h"

/* meaningless arguments for parcels */
struct args {
  double x;
  int y;
  char z;
};

hpx_future_t fut;

/*
 -------------------------------------------------------------------
  TEST HELPER: empty action for action/parcel tests
 -------------------------------------------------------------------
*/

void _test_action(void* args) {
  return;
}

/*
 -------------------------------------------------------------------
  TEST HELPER: worker for parcelqueue tests
 -------------------------------------------------------------------
*/

void thread_queue_worker(void* a) {
  hpx_parcelqueue_t *q = (hpx_parcelqueue_t*)a;
  int i;
  int success;
  hpx_parcel_t* val;
  for (i = 0; i < 7; i++) {
    val = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_parcelqueue_push(q, val);
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

void _test_action_checkrank(void* args) {
  unsigned int my_rank;
  my_rank = hpx_get_rank();
  ck_assert_msg(my_rank == 1, "Parcel sent to wrong locality");
  hpx_lco_future_set(&fut, 0);
}

/*
 --------------------------------------------------------------------
  TEST HELPER: Main thread for parcel send test
 --------------------------------------------------------------------
*/
void _thread_main_parcelsend(void* args) {
  int success;
  hpx_action_t* a;
  hpx_parcel_t* p;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_locality_t* other_loc;

  hpx_lco_future_init(&fut, 1);

  num_ranks = hpx_get_num_localities();
  ck_assert_msg(num_ranks > 1, "Couldn't send parcel - no remote localities available to send to");

  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  /* register action for parcel (must be done by all ranks) */
  a = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_test_action_checkrank", (hpx_func_t)_test_action_checkrank, a);

  if (my_rank == 0) {
    other_loc = hpx_get_locality(1);
    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel("_test_action_checkrank", (void*)NULL, 0, p);
    
    success = hpx_send_parcel(other_loc, p);
    ck_assert_msg(success == 0, "Couldn't send parcel");

    hpx_locality_destroy(other_loc);
  }
  else if (my_rank == 1) {
    hpx_thread_wait(&fut);
  }
  else {
  }

  /* cleanup */
  hpx_lco_future_destroy(&fut);

}


/*
 --------------------------------------------------------------------
  TEST HELPER: Test Runner for multi_thread_set
 --------------------------------------------------------------------
*/

void run_multi_thread_set_concurrent(uint32_t th_cnt, hpx_func_t th_func, void* args[]) {
  hpx_context_t * ctx;
  hpx_thread_t ** ths;
  hpx_config_t cfg;
  int idx;

  /* init our config */
  hpx_config_init(&cfg);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create HPX theads */
  ths = (hpx_thread_t **) hpx_alloc(sizeof(hpx_thread_t *) * th_cnt);
  ck_assert_msg(ths != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < th_cnt; idx++) {
    ths[idx] = hpx_thread_create(ctx, 0, th_func, args[idx]);
  }

  /* wait until our threads are done */
  for (idx = 0; idx < th_cnt; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }
  
  /* clean up */
  hpx_free(ths);
  hpx_ctx_destroy(ctx);
}



/*
 --------------------------------------------------------------------
  TEST: parcel handler queue init
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelqueue_create)
{
  bool success;
  hpx_parcelqueue_t* q;
  success = hpx_parcelqueue_create(&q);
  ck_assert_msg(success == 0, "Could not initialize parcelqueue");
  hpx_parcelqueue_destroy(&q);
  
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
  hpx_parcelqueue_t* q;

  success = hpx_parcelqueue_create(&q);

  hpx_parcel_t* vals[7];
  for (i = 0; i < 7; i++) {
    vals[i] = hpx_alloc(sizeof(hpx_parcel_t));
    memset(vals[i], 0, sizeof(hpx_parcel_t));
    success = hpx_parcelqueue_push(q, vals[i]);
    ck_assert_msg(success == 0, "Could not push to parcelqueue");
  }
  
  hpx_parcel_t* pop_vals[7];
  for (i = 0; i < 7; i++) {
    pop_vals[i] = (hpx_parcel_t*)hpx_parcelqueue_trypop(q);
    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
    ck_assert_msg(pop_vals[i] == vals[i], "Popped bad value from parcelqueue");
    hpx_free(pop_vals[i]);
  }
  
  hpx_parcelqueue_destroy(&q);

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
  hpx_parcelqueue_t* q;
  int count;
  count = 1000;
  hpx_parcelqueue_t* args[count];

  success = hpx_parcelqueue_create(&q);

  for (i = 0; i < count; i++)
    args[i] = q;

  run_multi_thread_set_concurrent(count, (hpx_func_t)thread_queue_worker, (void**)args);
  /* queue is full HERE */

  hpx_parcel_t* pop_vals[count];
  for (i = 0; i < count; i++) {
    pop_vals[i] = (hpx_parcel_t*)hpx_parcelqueue_trypop(q); /* normally this could be NULL, but we've already filled the queue, so we DON'T spin until non-NULL */
    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
    hpx_free(pop_vals[i]);
  }

  hpx_parcelqueue_destroy(&q);

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

  hpx_context_t * ctx;
  hpx_thread_t ** ths;
  hpx_config_t cfg;
  int idx;
  int i;
  int success;
  hpx_parcelqueue_t* q;
  hpx_thread_t** tids;
  hpx_parcel_t* pop_vals[count];
  for (i = 0; i < count; i++) {
    pop_vals[i] = NULL;
  }


  /* init our config */
  hpx_config_init(&cfg);

  /* get a thread context */
  ctx = hpx_ctx_create(&cfg);
  ck_assert_msg(ctx != NULL, "Could not get a thread context.");

  /* create parcelqueue */
  success = hpx_parcelqueue_create(&q);

  /* initialize arguments */

  /* create HPX theads */
  ths = (hpx_thread_t **) hpx_alloc(sizeof(hpx_thread_t *) * count);
  ck_assert_msg(ths != NULL, "Could not allocate an array to hold thread data.");

  for(idx = 0; idx < count; idx++) {
    ths[idx] = hpx_thread_create(ctx, 0, thread_queue_worker, (void*)q);
  }

  /* pop values from main thread BEFORE all other threads are finished */
  for (i = 0; i < count; i++) {
    while (pop_vals[i] == NULL) {
      pop_vals[i] = (hpx_parcel_t*)hpx_parcelqueue_trypop(q);
    }
    //    ck_assert_msg(pop_vals[i] != NULL, "Could not pop from parcelqueue");
  }

  /* wait until our threads are done */
  for (idx = 0; idx < count; idx++) {
    hpx_thread_join(ths[idx], NULL);
  }
  
  /* clean up */
  for (i = 0; i < count; i++) /* this is normally done right after popping, but we want to stress the queue so we wait */
    hpx_free(pop_vals[i]);
  hpx_free(ths);
  hpx_ctx_destroy(ctx);


}
END_TEST

/*
 --------------------------------------------------------------------
  TEST: parcel handler creation
 --------------------------------------------------------------------
*/

START_TEST (test_libhpx_parcelhandler_create)
{
  if (__hpx_parcelhandler == NULL) { /* if we've run init, this won't work, and besides we know parcelhandler_create works anyway because it's called by init() */
    hpx_parcelhandler_t * ph = NULL;
    
    ph = hpx_parcelhandler_create();
    ck_assert_msg(ph != NULL, "Could not create parcelhandler");
    
    hpx_parcelhandler_destroy(ph);

    ph = NULL;
  }
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
  hpx_action_t* a;
  a = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_test_action", (hpx_func_t)_test_action, a);

  ck_assert_msg(a != NULL, "Could not register action");
  ck_assert_msg(!strcmp(a->name, "_test_action"), "Error registering action - wrong name");
  ck_assert_msg(a->action == (hpx_func_t)_test_action, "Error registering action - wrong action");
		      
  free(a);
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
  hpx_action_t* a;
  hpx_parcel_t* p;
  int success;

  a = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_test_action", (hpx_func_t)_test_action, a);
  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel("_test_action", (void*)&Args, sizeof(struct args), p);

  ck_assert_msg(success == 0, "Could not create parcel");
  ck_assert_msg(p->action.action != NULL, "Error creating parcel - empty action");
  ck_assert_msg(p->action.action == (hpx_func_t)_test_action, "Error creating parcel - wrong action");

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
  struct args Args;
  Args.x = 1.414;
  Args.y = 3;
  Args.z = 'a';
  hpx_action_t* a;
  hpx_parcel_t* p;
  int success;
  char* blob;

  a = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_test_action", (hpx_func_t)_test_action, a);
  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel("_test_action", (void*)&Args, sizeof(struct args), p);

  ck_assert_msg(success == 0, "Could not serialize parcel - failed to create parcel");

  success = hpx_parcel_serialize(p, &blob);
  ck_assert_msg(success == 0, "Could not serialize parcel");

  int action_name_matches;
  action_name_matches = strncmp("_test_action", blob + sizeof(hpx_parcel_t), strlen("_test_action"));
  ck_assert_msg(action_name_matches == 0, "Parcel not serialized properly - action name was incorrect or missing");
  int payload_matches;
  payload_matches = memcmp((void*)&Args, (void*)(blob) + sizeof(hpx_parcel_t) + strlen("_test_action") + 1, sizeof(struct args));
  ck_assert_msg(action_name_matches == 0, "Parcel not serialized properly - action name was incorrect or missing");
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

  hpx_config_t *cfg;
  hpx_context_t *ctx;
  hpx_thread_t* th;

  //hpx_action_t* a_main;
  //hpx_action_t* a_sub; 
  //a_main = hpx_alloc(sizeof(hpx_action_t));
  //hpx_action_register("_thread_main_parcelsend", (hpx_func_t)_thread_main_parcelsend, a_main);

  cfg = hpx_alloc(sizeof(hpx_config_t));  
  hpx_config_init(cfg);
  ctx = hpx_ctx_create(cfg);
  th = hpx_thread_create(ctx, 0, (hpx_func_t)_thread_main_parcelsend, 0);
  hpx_thread_join(th, (void**)NULL);

  /* cleanup */
  hpx_ctx_destroy(ctx); /* note we don't need to free the context - destroy does that */
  hpx_free(cfg);
} 
END_TEST

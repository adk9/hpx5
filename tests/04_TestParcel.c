//****************************************************************************
// @Filename      04_TestParcel.c
// @Project       High Performance ParallelX Library (libhpx)
//----------------------------------------------------------------------------
// @Subject       Library Unit Test Harness - Memory Management
// 
// @Compiler      GCC
// @OS            Linux
// @Description   Tests the parcel funcitonalities
// @Goal          Goal of this testcase is to test the Parcels
//                1. hpx_parcel_aquire()             
//                2. hpx_parcel_set_target()
//                3. hpx_parcel_set_action()
//                4. hpx_parcel_set_data()
//                5. hpx_parcel_send_sync()                   
//                            
// @Copyright     Copyright (c) 2014, Trustees of Indiana University
//                All rights reserved.
//
//                This software may be modified and distributed under the terms
//                of the BSD license.  See the COPYING file for details.
//
//                This software was created at the Indiana University Center 
//                for Research in Extreme Scale Technologies (CREST).
//----------------------------------------------------------------------------
// @Date          08/21/2014
// @Author        Jayashree Candadai <jayaajay [at] indiana.edu>
// @Version       0.1
// Commands to Run: make, mpirun hpxtest 
//****************************************************************************

//****************************************************************************
// @Project Includes
//****************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "domain.h"

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

int t04_send_action(void *args) {
  int n = *(int*)args;
  printf("locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
         hpx_get_my_thread_id(), n);

  if (n-- <= 0) {
    printf("terminating.\n");
    return HPX_SUCCESS;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, t04_send);
  hpx_parcel_set_data(p, &n, sizeof(n));
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

//****************************************************************************
// Test code -- Parcels
//****************************************************************************
START_TEST (test_libhpx_parcelCreate)
{
  int n = 0;
  printf("Starting the parcel create test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t completed = hpx_lco_and_new(1);
  hpx_call(HPX_HERE, t04_send, &n, sizeof(n), completed);
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
}
END_TEST

//****************************************************************************
// Test code for parcel creation with arguments and parcel set and get action
//****************************************************************************

hpx_addr_t _partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return HPX_THERE((rank) ? 0 : ranks - 1);
}

int t04_sendData_action(const initBuffer_t *args) {
  printf("Received message = '%s', %d from (%d, %d)\n", args->message, 
         args->index, hpx_get_my_rank(), hpx_get_my_thread_id());
  return HPX_SUCCESS;
}

START_TEST (test_libhpx_parcelGetAction) 
{
  printf("Testing the parcel create with arguments\n");
  initBuffer_t args = {
    .index = hpx_get_my_rank(),
    .message = "parcel creation test"
  };

  hpx_addr_t to = _partner();
  
  hpx_parcel_t *p = hpx_parcel_acquire(&args, sizeof(args));
  hpx_parcel_set_action(p, t04_sendData);
  
  hpx_action_t get_act = hpx_parcel_get_action(p);
  ck_assert_msg(get_act == t04_sendData, "Error creating parcel - wrong action");

  hpx_parcel_set_data(p, &args, sizeof(args));
  hpx_parcel_set_target(p, to);

  hpx_parcel_send_sync(p); 
}
END_TEST

//****************************************************************************
// Test code to test parcel get data functions - The hpx_parcel_get_data gets
// the data buffer for a parcel. The data for a parcel can be written to
// directly, which in some cases may allow one to avoid an extra copy.
//****************************************************************************
START_TEST(test_libhpx_parcelGetData) 
{
  printf("Testing the parcel get data function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, t04_sendData);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel get data test");
  hpx_parcel_send_sync(p);
}
END_TEST

//****************************************************************************
// Testcase to test hpx_parcel_release function which explicitly releases a 
// a parcel. The input argument must correspond to a parcel pointer returned
// from hpx_parcel_acquire
//****************************************************************************
START_TEST(test_libhpx_parcelRelease)
{
  printf("Testing the parcel release function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, t04_sendData);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel release test");
  hpx_parcel_release(p);
}
END_TEST

//****************************************************************************
// This testcase tests hpx_parcel_send function, which sends a parcel with 
// asynchronout local completion symantics, hpx_parcel_set_cont_action - set
// the continuous action, hpx_pargel_set_cont_target - set the continuous 
// address for a parcel.
//****************************************************************************
int t04_recv_action(double *args) {
  return HPX_SUCCESS;
}

START_TEST(test_libhpx_parcelSend) 
{
  printf("Testing the hpx parcel send function\n");
  int buffer[4] = {1, 100, 1000, 10000};
  int avg = 1000;

  for (int i = 0; i < 4; i++) {
    size_t size = sizeof(double)*buffer[i];
    double *buf = malloc(size);
    for (int j = 0; j < buffer[i]; j++)
      buf[j] = rand() % 10000;

    printf("%d, %d, %g: " , i, buffer[i], buf[i]);
    hpx_time_t t1 = hpx_time_now();

    // Set the lco for completing the entire loop
    hpx_addr_t completed = hpx_lco_and_new(avg);
    
    for(int k = 0; k < avg; k++) {
      // Set up a asynchronous parcel send
      hpx_addr_t send = hpx_lco_future_new(0);
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
      hpx_parcel_set_action(p, t04_recv);
      hpx_parcel_set_target(p, HPX_HERE);
      hpx_parcel_set_cont_action(p, hpx_lco_set_action);
      hpx_parcel_set_cont_target(p, completed);
      hpx_parcel_set_data(p, buf, size);
      hpx_parcel_send(p, send);

      // do the useless work
      double volatile d = 0.;
      for (int i = 0; i < 1000; i++)
        d += 1./(2.*i+1.);

      hpx_lco_wait(send);
      hpx_lco_delete(send, HPX_NULL);
    }
    
    hpx_lco_wait(completed);
    hpx_lco_delete(completed, HPX_NULL);

    double elapsed = hpx_time_elapsed_ms(t1);
    printf("Elapsed: %g\n", elapsed/avg);
    free(buf);
  }  
}
END_TEST

//****************************************************************************
// Register tests from this file
//****************************************************************************

void add_04_TestParcel(TCase *tc) {
  tcase_add_test(tc, test_libhpx_parcelCreate);
  tcase_add_test(tc, test_libhpx_parcelGetAction);
  tcase_add_test(tc, test_libhpx_parcelGetData);
  tcase_add_test(tc, test_libhpx_parcelRelease);
  tcase_add_test(tc, test_libhpx_parcelSend);
}

// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

// Goal of this testcase is to test the Parcels
// 1.  hpx_parcel_aquire()
// 2.  hpx_parcel_set_target()
// 3.  hpx_parcel_set_action()
// 4.  hpx_parcel_set_data()
// 5.  hpx_parcel_send_sync()
// 6.  hpx_parcel_release()
// 7.  hpx_parcel_send()
// 8.  hpx_parcel_get_action()
// 9.  hpx_parcel_get_target()
// 10. hpx_parcel_get_cont_action()
// 11. hpx_parcel_get_cont_target()
// 12. hpx_parcel_get_data()
// 13. hpx_parcel_set_cont_action()
// 14. hpx_parcel_set_cont_target()

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "libsync/sync.h"

#define BUFFER_SIZE 128

typedef struct initBuffer {
  int  index;
  char message[BUFFER_SIZE];
} initBuffer_t;

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

static HPX_ACTION(_send, int *args) {
  int n = *args;
  //printf( "locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
  //       hpx_get_my_thread_id(), n);
  //printf("count: %d\n", n);
  if (n-- <= 0) {
    //printf("terminating.\n");
    return HPX_SUCCESS;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, _send);
  hpx_parcel_set_data(p, &n, sizeof(n));

  assert(hpx_gas_try_pin(hpx_parcel_get_target(p), NULL));

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Test code -- Parcels
static HPX_ACTION(test_libhpx_parcelCreate, void *UNUSED) {
  int n = 0;
  printf("Starting the parcel create test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t completed = hpx_lco_and_new(1);
  hpx_call(HPX_HERE, _send, completed, &n, sizeof(n));
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// Test code for parcel creation with arguments and parcel set and get action
hpx_addr_t _partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return HPX_THERE((rank) ? 0 : ranks - 1);
}

static HPX_ACTION(_sendData, const initBuffer_t *args) {
  //printf("Received message = '%s', %d from (%d, %d)\n", args->message,
  //       args->index, hpx_get_my_rank(), hpx_get_my_thread_id());
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_parcelGetAction, void *UNUSED) {
  printf("Testing the parcel create with arguments\n");
  initBuffer_t args = {
    .index = hpx_get_my_rank(),
    .message = "parcel creation test"
  };

  hpx_addr_t to = _partner();

  hpx_parcel_t *p = hpx_parcel_acquire(&args, sizeof(args));
  hpx_parcel_set_action(p, _sendData);

  hpx_action_t get_act = hpx_parcel_get_action(p);
  assert_msg(get_act == _sendData, "Error creating parcel - wrong action");

  hpx_parcel_set_data(p, &args, sizeof(args));
  hpx_parcel_set_target(p, to);

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Test code to test parcel get data functions - The hpx_parcel_get_data gets
// the data buffer for a parcel. The data for a parcel can be written to
// directly, which in some cases may allow one to avoid an extra copy.
static HPX_ACTION(test_libhpx_parcelGetData, void *UNUSED) {
  printf("Testing the parcel get data function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _sendData);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel get data test");
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Testcase to test hpx_parcel_release function which explicitly releases a
// a parcel. The input argument must correspond to a parcel pointer returned
// from hpx_parcel_acquire
static HPX_ACTION(test_libhpx_parcelRelease, void *UNUSED) {
  printf("Testing the parcel release function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _sendData);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel release test");
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}

static volatile int counter HPX_USED = 0;

// This testcase tests hpx_parcel_send function, which sends a parcel with
// asynchronout local completion symantics, hpx_parcel_set_cont_action - set
// the continuous action, hpx_pargel_set_cont_target - set the continuous
// address for a parcel.
static HPX_ACTION(_recv, double *args) {
  //printf("recv %d\n", sync_fadd(&counter, 1, SYNC_ACQ_REL));
  sync_fadd(&counter, 1, SYNC_ACQ_REL);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_parcelSend, void *UNUSED) {
  printf("Testing the hpx parcel send function\n");
  int buffer[4] = {1, 100, 1000, 10000};
  int avg = 1000;

  for (int i = 0; i < 4; i++) {
    size_t size = sizeof(double) * buffer[i];
    double *buf = malloc(size);
    for (int j = 0; j < buffer[i]; j++) {
      buf[j] = rand() % 10000;
    }

    printf("%d, %d, %g: " , i, buffer[i], buf[i]);
    hpx_time_t t1 = hpx_time_now();

    // Set the lco for completing the entire loop
    hpx_addr_t completed = hpx_lco_and_new(avg);

    for(int k = 0; k < avg; k++) {
      // Set up a asynchronous parcel send
      hpx_addr_t send = hpx_lco_future_new(0);
      hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
      hpx_parcel_set_action(p, _recv);
      hpx_parcel_set_target(p, HPX_HERE);
      hpx_parcel_set_cont_action(p, hpx_lco_set_action);
      hpx_parcel_set_cont_target(p, completed);
      hpx_parcel_set_data(p, buf, size);
      hpx_parcel_send(p, send);

      // do the useless work
      double volatile d = 0.;
      for (int i = 0; i < 1000; i++) {
        d += 1./(2.*i+1.);
      }

      hpx_lco_wait(send);
      hpx_lco_delete(send, HPX_NULL);
    }

    hpx_lco_wait(completed);
    hpx_lco_delete(completed, HPX_NULL);

    double elapsed = hpx_time_elapsed_ms(t1);
    printf("Elapsed: %g\n", elapsed/avg);
    free(buf);
  }
  return HPX_SUCCESS;
}

// Testcase to test the parcel continuation
static HPX_ACTION(_getContValue, uint64_t *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  uint64_t *value;
  if (!hpx_gas_try_pin(local, (void**)&value))
    return HPX_RESEND;

  memcpy(value, args, hpx_thread_current_args_size());
  //printf("Value =  %"PRIu64"\n", *value);

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static HPX_ACTION(test_libhpx_parcelGetContinuation, void *UNUSED) {
  printf("Testing parcel contination target and action\n");

  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t addr = hpx_gas_global_alloc(1, sizeof(uint64_t));

  hpx_addr_t done = hpx_lco_and_new(1);
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(uint64_t));

  // Get access to the data, and fill it with the necessary data.
  uint64_t *result = hpx_parcel_get_data(p);
  *result = 1234;

  // Set the target address and action for the parcel
  hpx_parcel_set_target(p, addr);
  hpx_parcel_set_action(p, _getContValue);

  // Set the continuation target and action for the parcel
  hpx_parcel_set_cont_target(p, done);
  hpx_parcel_set_cont_action(p, hpx_lco_set_action);

  hpx_action_t get_act = hpx_parcel_get_cont_action(p);
  assert_msg(get_act == hpx_lco_set_action,
                "Error in getting cont action");

  assert(hpx_parcel_get_cont_target(p) == done);

  // Send the parcel
  hpx_parcel_send(p, HPX_NULL);

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_free(addr, HPX_NULL);

  printf("Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

static int _is_error(hpx_status_t s) {
  if (s != HPX_SUCCESS) {
    fprintf(stderr, "HPX operation returned error %s", hpx_strerror(s));
  }
  return (s != HPX_SUCCESS);
}

static int _is_hpxnull(hpx_addr_t addr) {
  assert(addr != HPX_NULL);
  return (addr == HPX_NULL);
}

static int _is_null(void *addr) {
  assert(addr != NULL);
  return (addr == NULL);
}

/// Store a value to an integer in memory.
static HPX_PINNED(_store_int, void *val) {
  int *addr = hpx_thread_current_local_target();
  *addr = *(int*)val;
  return HPX_SUCCESS;
}

/// Load a value from an integer in memory.
static HPX_PINNED(_load_int, void) {
  int *addr = hpx_thread_current_local_target();
  HPX_THREAD_CONTINUE(*addr);
}

/// Increment an integer in memory.
///
/// This is slightly more complicated than an increment. We send along the value
/// that we expect to see, to make sure our cascade is running in the correct
/// order.
static HPX_PINNED(_parcelSendThrough_increment, void *arg) {
  // don't need synchronization since this is done in a sequential cascade
  int *val = hpx_thread_current_local_target();
  int i = val[0]++;
  int j = *(int*)arg;
  printf("expected %d, got %d\n", i, j);
  return HPX_SUCCESS;
}

/// This test sets up a simple cascade of parcels in a cyclic array of
/// futures. Each parcel waits for the future at i, then executes the
/// _test_libhpx_parcelSendThrough_increment on val, with the integer i, and
/// then triggers the future at i + 1.
hpx_addr_t _cascade(hpx_addr_t done, hpx_addr_t val, const int n) {
  // allocate the cascade array
  hpx_addr_t gates = hpx_lco_future_array_new(n, 0, 1);
  if (_is_hpxnull(gates)) {
    goto unwind0;
  }

  // set up the prefix of the cascade
  for (int i = 0, e = n; i < e; ++i) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(int));
    if (_is_null(p)) {
      goto unwind1;
    }

    // set up the initial action we want to run
    hpx_parcel_set_target(p, val);
    hpx_parcel_set_action(p, _parcelSendThrough_increment);
    hpx_parcel_set_data(p, &i, sizeof(i));

    // set up the continuation (trigger the next lco, or the done lco if we're
    // done)
    if (i < n - 1) {
      hpx_addr_t next = hpx_lco_future_array_at(gates, i + 1, 0, 1);
      hpx_parcel_set_cont_target(p, next);
    }
    else {
      hpx_parcel_set_cont_target(p, done);
    }
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // send the parcel through the current gate
    hpx_addr_t gate = hpx_lco_future_array_at(gates, i, 0, 1);
    if (_is_hpxnull(gate)) {
      goto unwind1;
    }

    if (_is_error(hpx_parcel_send_through_sync(p, gate))) {
      goto unwind1;
    }
  }

  return gates;

 unwind1:
  hpx_lco_delete(gates, HPX_NULL);
 unwind0:
  return HPX_NULL;
}

/// Test the parcel_send_though functionality.
///
/// The send-through operation is designed so that LCOs can buffer sent parcels
/// until their condition triggers. This permits a style of thread-less
/// synchronization that can aid in dataflow programming.
///
static HPX_ACTION(test_libhpx_parcelSendThrough, void *UNUSED) {
  const int n = 2 * HPX_LOCALITIES;

  printf("Testing parcel sends through LCOs\n");
  fflush(stdout);

  hpx_time_t t1 = hpx_time_now();

  // allocate a future to signal the completion of the cascade
  hpx_addr_t done = hpx_lco_future_new(0);
  if (_is_hpxnull(done)) {
    goto unwind0;
  }

  // allocate and initialize a shared integer for the cascade to update
  hpx_addr_t val = hpx_gas_alloc(sizeof(int));
  if (_is_hpxnull(val)) {
    goto unwind1;
  }

  const int zero = 0;
  if (_is_error(hpx_call_sync(val, _store_int, NULL, 0, &zero, sizeof(zero)))) {
    goto unwind2;
  }

  // allocate and initialize the cascade
  hpx_addr_t gates = _cascade(done, val, n);
  if (gates == HPX_NULL) {
    goto unwind2;
  }

  // start the cascade by setting the first future
  if (_is_error(hpx_call(gates, hpx_lco_set_action, HPX_NULL, NULL, 0))) {
    goto unwind2;
  }

  // wait for the cascade to finish
  if (_is_error(hpx_lco_wait(done))) {
    goto unwind3;
  }

  // check the final value to make sure everything actually worked
  int fin;
  if (_is_error(hpx_call_sync(val, _load_int, &fin, sizeof(fin), NULL, 0))) {
    goto unwind3;
  }

  if (fin != n) {
    fprintf(stderr, "expected final value %d, got %d", n, fin);
    return HPX_ERROR;
  }

 unwind3:
  hpx_lco_delete(gates, HPX_NULL);
 unwind2:
  hpx_gas_free(val, HPX_NULL);
 unwind1:
  hpx_lco_delete(done, HPX_NULL);
 unwind0:
  printf("Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  fflush(stdout);
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(test_libhpx_parcelCreate);
  ADD_TEST(test_libhpx_parcelGetAction);
  ADD_TEST(test_libhpx_parcelGetData);
  ADD_TEST(test_libhpx_parcelRelease);
  ADD_TEST(test_libhpx_parcelSend);
  ADD_TEST(test_libhpx_parcelGetContinuation);
  ADD_TEST(test_libhpx_parcelSendThrough);
});

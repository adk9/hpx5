// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include <inttypes.h>
#include <stdlib.h>
#include <hpx/hpx.h>
#include "tests.h"

enum { ELEMENTS = 32 };

static hpx_addr_t   _data = 0;
static hpx_addr_t _remote = 0;

static void HPX_NORETURN fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %" PRIu64 ", got %" PRIu64 "\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static int _set_handler(uint64_t *local, uint64_t value) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = value;
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _set, _set_handler,
                  HPX_POINTER, HPX_UINT64);

static int _verify_handler(uint64_t *local, uint64_t *args, size_t n) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != args[i]) {
      fail(i, args[i], local[i]);
    }
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _verify,
                  _verify_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int _init_globals_handler(void) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank + 1) % size;
  _data = hpx_gas_alloc_cyclic(size, n, 0);
  assert(_data);
  _remote = hpx_addr_add(_data, peer * n, n);
  assert(_remote);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _init_globals, _init_globals_handler);

static int _fini_globals_handler(void) {
  hpx_gas_free(_data, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _fini_globals, _fini_globals_handler);

static int _test_memput(uint64_t *local, hpx_addr_t done) {
  // clear the remote block
  uint64_t zero = 0;
  hpx_call_sync(_remote, _set, NULL, 0, &zero);

  // set up the local block
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = i;
  }

  // need two futures for the async version
  hpx_addr_t complete[2] = {
    hpx_lco_future_new(0),
    hpx_lco_future_new(0)
  };

  // perform the memput
  size_t n = ELEMENTS * sizeof(*local);
  hpx_gas_memput(_remote, local, n, complete[0], complete[1]);

  // wait for completion
  hpx_lco_wait_all(2, complete, NULL);
  hpx_lco_delete_all(2, complete, done);

  // and verify
  return hpx_call_sync(_remote, _verify, NULL, 0, local, n);
}

static int _test_memput_lsync(uint64_t *local, hpx_addr_t done) {
  // clear the remote block
  uint64_t zero = 0;
  hpx_call_sync(_remote, _set, NULL, 0, &zero);

  // set up the local block
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = i;
  }

  // need two futures for the async version
  hpx_addr_t lsync = hpx_lco_future_new(0);

  // perform the memput
  size_t n = ELEMENTS * sizeof(*local);
  hpx_gas_memput_lsync(_remote, local, n, lsync);

  // wait for completion
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, done);

  // and verify
  return hpx_call_sync(_remote, _verify, NULL, 0, local, n);
}


static int _test_memput_rsync(uint64_t *local) {
  // clear the remote block
  uint64_t zero = 0;
  hpx_call_sync(_remote, _set, NULL, 0, &zero);

  // set up the local block
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = i;
  }

  // perform the memput
  size_t n = ELEMENTS * sizeof(*local);
  hpx_gas_memput_rsync(_remote, local, n);

  // and verify
  return hpx_call_sync(_remote, _verify, NULL, 0, local, n);
}

static int gas_memput_stack_handler(void) {
  printf("Testing memput from a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  _test_memput(local, done);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_stack, gas_memput_stack_handler);

static int gas_memput_registered_handler(void) {
  printf("Testing memput from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  assert(local);
  hpx_addr_t done = hpx_lco_future_new(0);
  int e = _test_memput(local, done);
  hpx_free_registered(local);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_registered,
                  gas_memput_registered_handler);

static int gas_memput_malloc_handler(void) {
  printf("Testing memput from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  assert(local);
  hpx_addr_t done = hpx_lco_future_new(0);
  int e = _test_memput(local, done);
  free(local);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_malloc, gas_memput_malloc_handler);

static int gas_memput_global_handler(void) {
  printf("Testing memput from a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  _test_memput(local, done);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_global, gas_memput_global_handler);

static int gas_memput_lsync_stack_handler(void) {
  printf("Testing memput_lsync from a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  _test_memput_lsync(local, done);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_lsync_stack,
                  gas_memput_lsync_stack_handler);

static int gas_memput_lsync_registered_handler(void) {
  printf("Testing memput_lsync from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  assert(local);
  hpx_addr_t done = hpx_lco_future_new(0);
  int e = _test_memput_lsync(local, done);
  hpx_free_registered(local);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_lsync_registered,
                  gas_memput_lsync_registered_handler);

static int gas_memput_lsync_malloc_handler(void) {
  printf("Testing memput_lsync from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  assert(local);
  hpx_addr_t done = hpx_lco_future_new(0);
  int e = _test_memput_lsync(local, done);
  free(local);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_lsync_malloc,
                  gas_memput_lsync_malloc_handler);

static int gas_memput_lsync_global_handler(void) {
  printf("Testing memput_lsync from a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  _test_memput_lsync(local, done);
  hpx_lco_wait(done);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_lsync_global,
                  gas_memput_lsync_global_handler);

static int gas_memput_rsync_stack_handler(void) {
  printf("Testing memput_rsync from a stack address\n");
  uint64_t local[ELEMENTS];
  return _test_memput_rsync(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_rsync_stack,
                  gas_memput_rsync_stack_handler);

static int gas_memput_rsync_registered_handler(void) {
  printf("Testing memput_rsync from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  assert(local);
  int e = _test_memput_rsync(local);
  hpx_free_registered(local);
  return e;
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_rsync_registered,
                  gas_memput_rsync_registered_handler);

static int gas_memput_rsync_malloc_handler(void) {
  printf("Testing memput_rsync from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  assert(local);
  int e = _test_memput_rsync(local);
  free(local);
  return e;
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_rsync_malloc,
                  gas_memput_rsync_malloc_handler);

static int gas_memput_rsync_global_handler(void) {
  printf("Testing memput_rsync from a global address\n");
  static uint64_t local[ELEMENTS];
  return _test_memput_rsync(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memput_rsync_global,
                  gas_memput_rsync_global_handler);

TEST_MAIN({
    ADD_TEST(_init_globals);
    ADD_TEST(gas_memput_stack);
    ADD_TEST(gas_memput_lsync_stack);
    ADD_TEST(gas_memput_rsync_stack);
    ADD_TEST(gas_memput_registered);
    ADD_TEST(gas_memput_lsync_registered);
    ADD_TEST(gas_memput_rsync_registered);
    ADD_TEST(gas_memput_malloc);
    ADD_TEST(gas_memput_lsync_malloc);
    ADD_TEST(gas_memput_rsync_malloc);
    ADD_TEST(gas_memput_global);
    ADD_TEST(gas_memput_lsync_global);
    ADD_TEST(gas_memput_rsync_global);
    ADD_TEST(_fini_globals);
  });

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
static hpx_addr_t  _local = 0;
static hpx_addr_t _remote = 0;

static void
_fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %" PRIu64 ", got %" PRIu64 "\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static int
_reset_handler(uint64_t *local) {
  memset(local, 0, ELEMENTS * sizeof(*local));
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _reset, _reset_handler, HPX_POINTER);

static int
_verify_handler(uint64_t *local, uint64_t *args, size_t n) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != args[i]) {
      _fail(i, args[i], local[i]);
    }
  }
  hpx_call_cc(hpx_thread_current_target(), _reset, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED | HPX_MARSHALLED, _verify,
                  _verify_handler, HPX_POINTER, HPX_POINTER, HPX_SIZE_T);

static int
_init_globals_handler(void) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank + 1) % size;
  _data = hpx_gas_alloc_cyclic(size, n, 0);
  test_assert(_data != HPX_NULL);

  _local = _data;
  CHECK( hpx_call_sync(_local, _reset, NULL, 0) );

  _remote = hpx_addr_add(_data, peer * n, n);
  CHECK( hpx_call_sync(_remote, _reset, NULL, 0) );

  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _init_globals, _init_globals_handler);

static int
_fini_globals_handler(void) {
  hpx_gas_free(_data, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _fini_globals, _fini_globals_handler);

static int
_test_memput(uint64_t *local, hpx_addr_t block, hpx_addr_t done) {
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
  CHECK( hpx_gas_memput(block, local, n, complete[0], complete[1]) );

  // wait for completion
  CHECK( hpx_lco_wait_all(2, complete, NULL) );
  hpx_lco_delete_all(2, complete, done);

  // and verify
  return hpx_call_sync(block, _verify, NULL, 0, local, n);
}

static int
_test_memput_lsync(uint64_t *local, hpx_addr_t block, hpx_addr_t done) {
  // set up the local block
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = i;
  }

  // need two futures for the async version
  hpx_addr_t lsync = hpx_lco_future_new(0);
  test_assert(lsync != HPX_NULL);

  // perform the memput
  size_t n = ELEMENTS * sizeof(*local);
  CHECK( hpx_gas_memput_lsync(block, local, n, lsync) );

  // wait for completion
  CHECK( hpx_lco_wait(lsync) );
  hpx_lco_delete(lsync, done);

  // and verify
  return hpx_call_sync(block, _verify, NULL, 0, local, n);
}


static int
_test_memput_rsync(uint64_t *local, hpx_addr_t block) {
  // set up the local block
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    local[i] = i;
  }

  // perform the memput
  size_t n = ELEMENTS * sizeof(*local);
  CHECK( hpx_gas_memput_rsync(block, local, n) );

  // and verify
  return hpx_call_sync(block, _verify, NULL, 0, local, n);
}

static int
_memput_local_handler(void) {
  printf("Testing gas_memput to a local block\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput(local, _local, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_local, _memput_local_handler);

static int
_memput_stack_handler(void) {
  printf("Testing gas_memput from a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_stack, _memput_stack_handler);

static int
_memput_registered_handler(void) {
  printf("Testing gas_memput from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  test_assert(local != NULL);
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);

  _test_memput(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_free_registered(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_registered,
                  _memput_registered_handler);

static int
_memput_malloc_handler(void) {
  printf("Testing gas_memput from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  test_assert(local);
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  free(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_malloc, _memput_malloc_handler);

static
int _memput_global_handler(void) {
  printf("Testing gas_memput from a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_global, _memput_global_handler);

static int
_memput_lsync_local_handler(void) {
  printf("Testing gas_memput_lsync to a local block\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput_lsync(local, _local, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_lsync_local,
                  _memput_lsync_local_handler);

static int
_memput_lsync_stack_handler(void) {
  printf("Testing gas_memput_lsync from a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput_lsync(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_lsync_stack,
                  _memput_lsync_stack_handler);

static int
_memput_lsync_registered_handler(void) {
  printf("Testing gas_memput_lsync from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  test_assert(local != NULL);
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput_lsync(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_free_registered(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_lsync_registered,
                  _memput_lsync_registered_handler);

static int
_memput_lsync_malloc_handler(void) {
  printf("Testing gas_memput_lsync from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  test_assert(local != NULL);
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput_lsync(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  free(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_lsync_malloc,
                  _memput_lsync_malloc_handler);

static int
_memput_lsync_global_handler(void) {
  printf("Testing gas_memput_lsync from a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);
  _test_memput_lsync(local, _remote, done);
  CHECK( hpx_lco_wait(done) );
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_lsync_global,
                  _memput_lsync_global_handler);

static int
_memput_rsync_local_handler(void) {
  printf("Testing gas_memput_rsync to a local block\n");
  uint64_t local[ELEMENTS];
  return _test_memput_rsync(local, _local);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_rsync_local,
                  _memput_rsync_local_handler);

static int
_memput_rsync_stack_handler(void) {
  printf("Testing gas_memput_rsync from a stack address\n");
  uint64_t local[ELEMENTS];
  return _test_memput_rsync(local, _remote);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_rsync_stack,
                  _memput_rsync_stack_handler);

static int
_memput_rsync_registered_handler(void) {
  printf("Testing gas_memput_rsync from a registered address\n");
  uint64_t *local = hpx_malloc_registered(ELEMENTS * sizeof(*local));
  test_assert(local != NULL);
  int e = _test_memput_rsync(local, _remote);
  hpx_free_registered(local);
  return e;
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_rsync_registered,
                  _memput_rsync_registered_handler);

static int
_memput_rsync_malloc_handler(void) {
  printf("Testing gas_memput_rsync from a malloced address\n");
  uint64_t *local = calloc(ELEMENTS, sizeof(*local));
  test_assert(local != NULL);
  int e = _test_memput_rsync(local, _remote);
  free(local);
  return e;
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_rsync_malloc,
                  _memput_rsync_malloc_handler);

static int
_memput_rsync_global_handler(void) {
  printf("Testing gas_memput_rsync from a global address\n");
  static uint64_t local[ELEMENTS];
  return _test_memput_rsync(local, _remote);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memput_rsync_global,
                  _memput_rsync_global_handler);

TEST_MAIN({
    ADD_TEST(_init_globals);
    ADD_TEST(_memput_local);
    ADD_TEST(_memput_lsync_local);
    ADD_TEST(_memput_rsync_local);
    ADD_TEST(_memput_stack);
    ADD_TEST(_memput_lsync_stack);
    ADD_TEST(_memput_rsync_stack);
    ADD_TEST(_memput_registered);
    ADD_TEST(_memput_lsync_registered);
    ADD_TEST(_memput_rsync_registered);
    ADD_TEST(_memput_malloc);
    ADD_TEST(_memput_lsync_malloc);
    ADD_TEST(_memput_rsync_malloc);
    ADD_TEST(_memput_global);
    ADD_TEST(_memput_lsync_global);
    ADD_TEST(_memput_rsync_global);
    ADD_TEST(_fini_globals);
  });

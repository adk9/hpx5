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
#include "hpx/hpx.h"
#include "tests.h"

// Size of the data we're transferring.
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
_verify(uint64_t *local) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != i) {
      _fail(i, i, local[i]);
    }
  }
  return HPX_SUCCESS;
}

/// Initialize the global data for a rank.
static int
_init_handler(hpx_addr_t data) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  int rank = HPX_LOCALITY_ID;
  int peer = (rank + 1) % HPX_LOCALITIES;

  _data = data;
  _local = hpx_addr_add(data, rank * n, n);
  _remote = hpx_addr_add(data, peer * n, n);

  uint64_t *local;
  test_assert( hpx_gas_try_pin(_local, (void**)&local) );
  printf("initializing %"PRIu64" (%p at %d)\n", _local, (void*)local, rank);
  for (int i = 0; i < ELEMENTS; ++i) {
    local[i] = i;
  }
  hpx_gas_unpin(_local);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _init, _init_handler, HPX_ADDR);

static int
_init_globals_handler(void) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  hpx_addr_t data = hpx_gas_alloc_cyclic(HPX_LOCALITIES, n, 0);
  test_assert_msg(data != HPX_NULL, "failed to allocate data\n");

  CHECK( hpx_bcast_rsync(_init, &data) );
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _init_globals, _init_globals_handler);

static int
_fini_globals_handler(void) {
  hpx_gas_free_sync(_data);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _fini_globals, _fini_globals_handler);

static int
_memget_local_handler(void) {
  printf("Testing gas_memget from a local block (from %"PRIu64")\n", _local);
  static uint64_t local[ELEMENTS] = {0};
  hpx_addr_t done = hpx_lco_future_new(0);
  CHECK( hpx_gas_memget(local, _local, sizeof(local), done) );
  CHECK( hpx_lco_wait(done) );
  _verify(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_local, _memget_local_handler);

static int
_memget_sync_local_handler(void) {
  printf("Testing gas_memget_sync from a local block (from %"PRIu64")\n", _local);
  static uint64_t local[ELEMENTS] = {0};
  CHECK( hpx_gas_memget_sync(local, _local, sizeof(local)) );
  return _verify(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_sync_local,
                  _memget_sync_local_handler);

static int
_memget_sync_stack_handler(void) {
  printf("Testing gas_memget_sync to a stack address (from %"PRIu64")\n", _remote);
  uint64_t local[ELEMENTS];
  CHECK( hpx_gas_memget_sync(local, _remote, sizeof(local)) );
  return _verify(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_sync_stack,
                  _memget_sync_stack_handler);

static int _memget_sync_registered_handler(void) {
  printf("Testing gas_memget_sync to a registered address (from %"PRIu64")\n", _remote);
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = hpx_malloc_registered(n);
  test_assert(local);
  CHECK( hpx_gas_memget_sync(local, _remote, n) );
  _verify(local);
  hpx_free_registered(local);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_sync_registered,
                  _memget_sync_registered_handler);

static int _memget_sync_global_handler(void) {
  printf("Testing gas_memget_sync to a global address (from %"PRIu64")\n", _remote);
  static uint64_t local[ELEMENTS] = {0};
  CHECK( hpx_gas_memget_sync(local, _remote, sizeof(local)) );
  return _verify(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_sync_global,
                  _memget_sync_global_handler);

static int _memget_sync_malloc_handler(void) {
  printf("Testing gas_memget_sync to a malloced address (from %"PRIu64")\n", _remote);
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = calloc(1, n);
  test_assert(local);
  CHECK( hpx_gas_memget_sync(local, _remote, n) );
  _verify(local);
  free(local);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_sync_malloc,
                  _memget_sync_malloc_handler);

static int _memget_stack_handler(void) {
  printf("Testing gas_memget to a stack address (from %"PRIu64")\n", _remote);
  uint64_t local[ELEMENTS] = {0};
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);

  CHECK( hpx_gas_memget(local, _remote, sizeof(local), done) );
  CHECK( hpx_lco_wait(done) );
  _verify(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_stack,
                  _memget_stack_handler);


static int _memget_registered_handler(void) {
  printf("Testing gas_memget to a registered address (from %"PRIu64")\n", _remote);
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = hpx_malloc_registered(n);
  test_assert(local != NULL);

  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);

  CHECK( hpx_gas_memget(local, _remote, n, done) );
  CHECK( hpx_lco_wait(done) );
  _verify(local);
  hpx_free_registered(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_registered,
                  _memget_registered_handler);

static int _memget_global_handler(void) {
  printf("Testing gas_memget to a global address (from %"PRIu64")\n", _remote);
  static uint64_t local[ELEMENTS] = {0};
  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);

  CHECK( hpx_gas_memget(local, _remote, sizeof(local), done) );
  CHECK( hpx_lco_wait(done) );
  _verify(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_global,
                  _memget_global_handler);

static int _memget_malloc_handler(void) {
  printf("Testing gas_memget to a malloced address (from %"PRIu64")\n", _remote);
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = calloc(1, n);
  test_assert(local != NULL);

  hpx_addr_t done = hpx_lco_future_new(0);
  test_assert(done != HPX_NULL);

  CHECK( hpx_gas_memget(local, _remote, n, done) );
  CHECK( hpx_lco_wait(done) );
  _verify(local);
  free(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, _memget_malloc,
                  _memget_malloc_handler);

TEST_MAIN({
    ADD_TEST(_init_globals, 0);
    ADD_TEST(_memget_local, 0);
    ADD_TEST(_memget_local, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_sync_local, 0);
    ADD_TEST(_memget_sync_local, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_stack, 0);
    ADD_TEST(_memget_stack, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_sync_stack, 0);
    ADD_TEST(_memget_sync_stack, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_registered, 0);
    ADD_TEST(_memget_registered, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_sync_registered, 0);
    ADD_TEST(_memget_sync_registered, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_global, 0);
    ADD_TEST(_memget_global, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_sync_global, 0);
    ADD_TEST(_memget_sync_global, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_malloc, 0);
    ADD_TEST(_memget_malloc, 1 % HPX_LOCALITIES);
    ADD_TEST(_memget_sync_malloc, 0);
    ADD_TEST(_memget_sync_malloc, 1 % HPX_LOCALITIES);
    ADD_TEST(_fini_globals, 0);
  });

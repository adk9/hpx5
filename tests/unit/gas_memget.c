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
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

// Size of the data we're transferring.
enum { ELEMENTS = 32 };

static hpx_addr_t   _data = 0;
static hpx_addr_t _remote = 0;

static void HPX_NORETURN _fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %lu, got %lu\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static int _verify(uint64_t *local) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != i) {
      _fail(i, i, local[i]);
    }
  }
  return HPX_SUCCESS;
}

static HPX_PINNED(_init, uint64_t *local, void* args) {
  for (int i = 0; i < ELEMENTS; ++i) {
    local[i] = i;
  }
  return HPX_SUCCESS;
}

static HPX_ACTION(_init_globals, void *UNUSED) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank + 1) % size;
  _data = hpx_gas_alloc_cyclic(HPX_LOCALITIES, n);
  assert(_data);
  _remote = hpx_addr_add(_data, peer * n, n);
  assert(_remote);
  return hpx_call_sync(_remote, _init, NULL, 0, NULL, 0);
}

static HPX_ACTION(_fini_globals, void *UNUSED) {
  hpx_gas_free(_data, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(gas_memget_stack, void *UNUSED) {
  printf("Testing memget to a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_gas_memget_sync(local, _remote, sizeof(local));
  return _verify(local);
}

static HPX_ACTION(gas_memget_registered, void *UNUSED) {
  printf("Testing memget to a registered address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = hpx_malloc_registered(n);
  hpx_gas_memget_sync(local, _remote, n);
  _verify(local);
  hpx_free_registered(local);
  return HPX_SUCCESS;
}

static HPX_ACTION(gas_memget_global, void *UNUSED) {
  printf("Testing memget to a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_gas_memget_sync(local, _remote, sizeof(local));
  return _verify(local);
}

static HPX_ACTION(gas_memget_malloc, void *UNUSED) {
  printf("Testing memget to a malloced address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = malloc(n);
  hpx_gas_memget_sync(local, _remote, n);
  _verify(local);
  free(local);
  return HPX_SUCCESS;
}

TEST_MAIN({
    ADD_TEST(_init_globals);
    ADD_TEST(gas_memget_stack);
    ADD_TEST(gas_memget_registered);
    ADD_TEST(gas_memget_global);
    ADD_TEST(gas_memget_malloc);
    ADD_TEST(_fini_globals);
  });

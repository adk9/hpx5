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

// Goal of this testcase is to test collectives

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"

typedef struct Domain {
  hpx_addr_t newdt;
  hpx_addr_t complete;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

typedef struct {
  int           nDoms;
  int       maxcycles;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

/// Initialize a double zero.
static void _initDouble(double *input, const size_t bytes) {
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void _maxDouble(double *lhs, const double *rhs, const size_t bytes) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

static HPX_PINNED(_initDomain, Domain *ld, const InitArgs *args) {
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;
  ld->complete = args->complete;
  ld->cycle = 0;

  // record the newdt allgather
  ld->newdt = args->newdt;
  return HPX_SUCCESS;
}

static HPX_ACTION(_advanceDomain_reduce, const unsigned long *epoch) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain)) {
    return HPX_RESEND;
  }

  if (domain->cycle >= domain->maxcycles) {
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the reduce
  double gnewdt = (domain->cycle == 0) ? 3141592653.58979 :
      3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_set(domain->newdt, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);

  domain->cycle += 1;
  //const unsigned long next = *epoch + 1;
  hpx_gas_unpin(local);
  //return hpx_call(local, _advanceDomain_reduce, HPX_NULL, &next, sizeof(next));
  return HPX_SUCCESS;
}

static HPX_ACTION(lco_reduce, void *UNUSED) {
  int nDoms = 8;
  int maxCycles = 100;

  hpx_addr_t domain = hpx_gas_calloc_cyclic(nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(nDoms);

  hpx_addr_t newdt = hpx_lco_reduce_new(nDoms, sizeof(double),
                                        (hpx_monoid_id_t)_initDouble,
                                        (hpx_monoid_op_t)_maxDouble);

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .nDoms = nDoms,
      .maxcycles = maxCycles,
      .complete = HPX_NULL,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;

  for (int i = 0, e = nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _advanceDomain_reduce, HPX_NULL, &epoch, sizeof(epoch));
  }

  // Get the gathered value, and print the debugging string.
  double ans;
  hpx_lco_get(newdt, sizeof(double), &ans);
  assert(ans == 3141592653.58979);

  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);

  return HPX_SUCCESS;
}

static HPX_ACTION(_advanceDomain_allreduce, const unsigned long *epoch) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the allreduce
  double gnewdt = 3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_set(domain->newdt, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  double newdt;
  hpx_lco_get(domain->newdt, sizeof(double), &newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain_allreduce, HPX_NULL, &next, sizeof(next));
}

static HPX_ACTION(lco_allreduce, void *UNUSED) {
  int nDoms = 8;
  int maxCycles = 100;

  hpx_addr_t domain = hpx_gas_calloc_cyclic(nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  hpx_addr_t newdt = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(double),
                                           (hpx_monoid_id_t)_initDouble,
                                           (hpx_monoid_op_t)_maxDouble);

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .nDoms = nDoms,
      .maxcycles = maxCycles,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;

  for (int i = 0, e = nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _advanceDomain_allreduce, HPX_NULL, &epoch, sizeof(epoch));
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);

  return HPX_SUCCESS;
}

static HPX_ACTION(_advanceDomain_allgather, const unsigned long *epoch) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the allgather
  double gnewdt = 3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_allgather_setid(domain->newdt, domain->rank, sizeof(double), &gnewdt,
                          HPX_NULL, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  double newdt[domain->nDoms];
  hpx_lco_get(domain->newdt, sizeof(newdt), &newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain_allgather, HPX_NULL, &next, sizeof(next));
}

static HPX_ACTION(lco_allgather, void *UNUSED) {
  int nDoms = 8;
  int maxCycles = 100;

  hpx_addr_t domain = hpx_gas_calloc_cyclic(nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  hpx_addr_t newdt = hpx_lco_allgather_new(nDoms, sizeof(double));

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .nDoms = nDoms,
      .maxcycles = maxCycles,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;
  for (int i = 0, e = nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _advanceDomain_allgather, HPX_NULL, &epoch, sizeof(epoch));
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);
  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);
  return HPX_SUCCESS;
}

static HPX_ACTION(_advanceDomain_alltoall, const unsigned long *epoch) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain)) {
    return HPX_RESEND;
  }

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the alltoall
  int gnewdt[domain->nDoms];
  for (int k=0; k < domain->nDoms; k++) {
    gnewdt[k] = (k+1) + domain->rank * domain->nDoms;
  }

  hpx_lco_alltoall_setid(domain->newdt, domain->rank,
                         domain->nDoms * sizeof(int),
                         gnewdt, HPX_NULL, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  int newdt[domain->nDoms];
  hpx_lco_alltoall_getid(domain->newdt, domain->rank, sizeof(newdt), &newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain_alltoall, HPX_NULL, &next, sizeof(next));
}

static HPX_ACTION(lco_alltoall, void *UNUSED) {
  int nDoms = 8;
  int maxCycles = 100;

  hpx_addr_t domain = hpx_gas_calloc_cyclic(nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  hpx_addr_t newdt = hpx_lco_alltoall_new(nDoms, nDoms * sizeof(int));

  for (int i = 0, e = nDoms; i < e; ++i) {
    InitArgs init = {
      .nDoms = nDoms,
      .maxcycles = maxCycles,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;
  for (int i = 0, e = nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _advanceDomain_alltoall, HPX_NULL, &epoch, sizeof(epoch));
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);
  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(lco_reduce);
  ADD_TEST(lco_allreduce);
  ADD_TEST(lco_allgather);
  ADD_TEST(lco_alltoall);
});

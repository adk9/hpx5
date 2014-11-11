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

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"


typedef struct {
  int nDoms;
  int maxCycles;
} main_args_t;

typedef struct Domain {
  hpx_addr_t complete;
  hpx_addr_t newdt;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

int allreduce_main_action(const main_args_t *args);

void allreduce_init_actions(void);
static hpx_action_t _main = 0;
static hpx_action_t _initDomain = 0;
static hpx_action_t _advanceDomain = 0;

static void
_usage(FILE *f, int error) {
  fprintf(f, "Usage: ./allreduce-bench [options] [CYCLES]\n"
          "\t-n, number of domains\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
  exit(error);
}

/// Initialize a double zero.
static void
_initDouble(double *input)
{
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void
_maxDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

static int
_initDomain_action(const InitArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = args->index;
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;
  ld->complete = args->complete;
  ld->cycle = 0;

  // record the newdt allgather
  ld->newdt = args->newdt;

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

static int
_advanceDomain_action(const unsigned long *epoch)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    printf("Finished processing %lu epochs at domain %u\n", *epoch, domain->rank);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the allreduce
  double gnewdt = 3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_set(domain->newdt, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  double newdt;
  hpx_lco_get(domain->newdt, sizeof(double), &newdt);
  printf(" TEST cycle %d rank %d newdt %g\n", domain->cycle, domain->rank,
         newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
}

int
allreduce_main_action(const main_args_t *args)
{
  hpx_time_t t1 = hpx_time_now();

  printf("Number of domains: %d maxCycles: %d\n",
         args->nDoms, args->maxCycles);
  fflush(stdout);

  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(args->nDoms);
  hpx_addr_t complete = hpx_lco_and_new(args->nDoms);

  // Call the allreduce function here.
  hpx_addr_t newdt = hpx_lco_allreduce_new(args->nDoms, args->nDoms, sizeof(double),
                                           (hpx_commutative_associative_op_t)_maxDouble,
                                           (void (*)(void *, const size_t size)) _initDouble);

  for (int i = 0, e = args->nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _initDomain, &init, sizeof(init), done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;
  for (int i = 0, e = args->nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, sizeof(Domain));
    hpx_call(block, _advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_gas_free(domain, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_shutdown(0);
}

/// Register the actions that we need.
void
allreduce_init_actions(void)
{
  HPX_REGISTER_ACTION(&_main, allreduce_main_action);
  HPX_REGISTER_ACTION(&_initDomain, _initDomain_action);
  HPX_REGISTER_ACTION(&_advanceDomain, _advanceDomain_action);
}

int
main(int argc, char *argv[argc])
{
  // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
  };

  // initialize HPX
  int err = hpx_init(&argc, &argv);
  if (err)
    return err;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "n:h?")) != -1) {
    switch (opt) {
     case 'n':
       args.nDoms = atoi(optarg);
       break;
     case 'h':
       _usage(stdout, EXIT_SUCCESS);
     case '?':
     default:
       _usage(stderr, EXIT_FAILURE);
    }
  }

  argc -= optind;
  argv += optind;

  switch (argc) {
   case 1:
     args.maxCycles = atoi(argv[0]);
     break;
   case 0:
    break;
   default:
     _usage(stderr, EXIT_FAILURE);
  }

  // register HPX actions
  allreduce_init_actions();

  // run HPX (this copies the args structure)
  return hpx_run(&_main, &args, sizeof(args));
}



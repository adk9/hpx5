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
#include "common.h"
#include "libsync/sync.h"

#define BENCHMARK "HPX COST OF AllGather LCO (us)"
#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
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
  int           cores;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

int allgather_main_action(const main_args_t *args);

void allgather_init_actions(void);
static hpx_action_t _initDomain = 0;
static hpx_action_t _advanceDomain = 0;

static void
_usage(FILE *f) {
  fprintf(f, "Usage: ./example [options] [CYCLES]\n"
           "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, set logging level\n"
          "\t-s, set stack size\n"
          "\t-p, set per-PE global heap size\n"
          "\t-r, set send/receive request limit\n"
          "\t-h, this help display\n");
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

  fflush(stdout);
  return HPX_SUCCESS;
}

unsigned long timeSet = 0.0;
unsigned long timeGet = 0.0;

static int
_advanceDomain_action(const unsigned long *epoch)
{
  hpx_time_t t;
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
  t = hpx_time_now();
  hpx_lco_allgather_setid(domain->newdt, domain->rank, sizeof(double), &gnewdt,
                          HPX_NULL, HPX_NULL);
  sync_addf(&timeSet, (unsigned long)(hpx_time_elapsed_ms(t)*1000), SYNC_RELAXED);

  // Get the gathered value, and print the debugging string.
  double newdt[domain->nDoms];
  t = hpx_time_now();
  hpx_lco_get(domain->newdt, sizeof(newdt), &newdt);
  sync_addf(&timeGet, (unsigned long)(hpx_time_elapsed_ms(t)*1000), SYNC_RELAXED);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
}

int
allgather_main_action(const main_args_t *args)
{
  hpx_time_t t1;

  fprintf(test_log, HEADER);
  fprintf(test_log, "%s%*s%*s%*s\n", "# Iters " , FIELD_WIDTH,
           "Init time ", FIELD_WIDTH, "LCO Set", FIELD_WIDTH, "LCO Get");
  fprintf(test_log, "%d\t", args->maxCycles);

  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(args->nDoms);
  hpx_addr_t complete = hpx_lco_and_new(args->nDoms);

  // Call the allgather function here.
  t1 = hpx_time_now();
  hpx_addr_t newdt = hpx_lco_allgather_new(args->nDoms, sizeof(double));
  fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t1));

  for (int i = 0, e = args->nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .cores = args->cores,
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

  fprintf(test_log, "%*lu", FIELD_WIDTH, timeSet);
  fprintf(test_log, "%*lu\n", FIELD_WIDTH, timeGet);

  hpx_shutdown(0);
}

/// Register the actions that we need.
void
allgather_init_actions(void)
{
  _initDomain = HPX_REGISTER_ACTION(_initDomain_action);
  _advanceDomain = HPX_REGISTER_ACTION(_advanceDomain_action);
}

int
main(int argc, char * const argv[argc])
{
  // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
    .cores = 8
  };

  // allocate the default HPX configuration on the stack
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dl:s:p:r:q:h")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'l':
      cfg.log_level = atoi(optarg);
      break;
     case 's':
      cfg.stack_bytes = strtoul(optarg, NULL, 0);
      break;
     case 'p':
      cfg.heap_bytes = strtoul(optarg, NULL, 0);
      break;
     case 'r':
      cfg.req_limit = strtoul(optarg, NULL, 0);
      break;
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }


  // initialize HPX
  int err = hpx_init(&cfg);
  if (err)
    return err;

  argc -= optind;
  argv += optind;

  switch (argc) {
   case 1:
    args.maxCycles = atoi(argv[0]);
    break;
   case 0:
    break;
   default:
    _usage(stderr);
    return -1;
  }

  test_log = fopen("test.log", "a+");

  // register HPX actions
  allgather_init_actions();

  // register the main action
  hpx_action_t _main = HPX_REGISTER_ACTION(allgather_main_action);

  // run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}



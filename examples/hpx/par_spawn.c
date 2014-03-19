/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Parallel spawn
  examples/hpx/par_spawn.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#include <argp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "hpx/hpx.h"
#include <sync/sync.h>

/**
 * This file defines a parallel (tree) spawn, that uses HPX threads to
 * spawn NOP operations.
 */

static hpx_action_t nop = 0;
static hpx_action_t par_for = 0;
static hpx_action_t par_main = 0;

#define SEQ_CUTOFF 1000

static int nthreads;                           /**< the number of threads  */

/// The command line arguments for fibonacci
typedef struct {
  int n;
  int debug;
  int threads;
} args_t;

/// The options that fibonacci understands.
static struct argp_option opts[] = {
  {"debug", 'd', 0, 0, "Wait for the debugger"},
  {"threads", 't', "HPX_THREADS", 0, "HPX scheduler threads"},
  { 0 }
};

static char doc[] = "par_spawn: A parallel spawn microbenchmark.";
static char args_doc[] = "ARG1";

// Our argument parser.
static int parse(int key, char *arg, struct argp_state *state) {
  args_t *args = (args_t*)state->input;

  switch (key) {
   default:
    return ARGP_ERR_UNKNOWN;

   case 'd':
    args->debug = 1;
    break;

   case 't':
    args->threads = atoi(arg);
    break;

   case ARGP_KEY_NO_ARGS:
    argp_usage(state);
    break;

   case ARGP_KEY_ARG:
    if (state->arg_num > 1)
      argp_usage(state);

    if (!arg)
      abort();

    args->n = atoi(arg);
    break;
  }
  return 0;
}

/**
 * The empty action
 */
static int
nop_action(void *args)
{
  return sync_fadd(&nthreads, 1, SYNC_SEQ_CST);
}

static int
par_for_action(void *args) {
  int n = *(int*)args;
  hpx_addr_t addr = hpx_addr_from_rank(hpx_get_my_rank());

  if (n <= SEQ_CUTOFF) {
    for (int i = 0; i < SEQ_CUTOFF; i++)
      hpx_call(addr, nop, 0, 0, HPX_NULL);
    return 0;
  }

  int nn = n/4;
  hpx_call(addr, par_for, &nn, sizeof(nn), HPX_NULL);
  hpx_call(addr, par_for, &nn, sizeof(nn), HPX_NULL);
  hpx_call(addr, par_for, &nn, sizeof(nn), HPX_NULL);
  nn = n - (n/4)*3;
  hpx_call(addr, par_for, &nn, sizeof(nn), HPX_NULL);
  return HPX_SUCCESS;
}

static int
par_main_action(void *args) {
  int n = *(int*)args;
  hpx_addr_t addr = hpx_addr_from_rank(hpx_get_my_rank());
  printf("par_spawn(%d)\n", n); fflush(stdout);
  hpx_time_t clock = hpx_time_now();
  hpx_call(addr, par_for, &n, sizeof(n), HPX_NULL);
  do {
    hpx_yield();
  } while (nthreads < n);

  double time = hpx_time_elapsed_ms(clock)/1e3;

  printf("seconds: %.7f\n", time);
  printf("localities:   %d\n", hpx_get_num_ranks());
  printf("threads:      %d\n", hpx_get_num_threads());
  hpx_shutdown(0);
  return HPX_SUCCESS;
}

/**
 * The main function parses the command line, sets up the HPX runtime system,
 * and initiates the first HPX thread to perform par_spawn(n).
 *
 * @param argc number of strings
 * @param argv[0] par_spawn
 * @param argv[1] number of cores to use, '0' means use all
 * @param argv[2] n
 */
int
main(int argc, char *argv[])
{
  args_t args = {0};
  struct argp parser = { opts, parse, args_doc, doc };
  int e = argp_parse(&parser, argc, argv, 0, 0, &args);
  if (e)
    return e;

  hpx_config_t config = {
    .scheduler_threads = args.threads,
    .stack_bytes = 0
  };

  if (args.debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
  }

  /* initialize thread count */
  nthreads = 0;

  e = hpx_init(&config);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // register the fib action
  nop = hpx_action_register("nop", nop_action);
  par_main = hpx_action_register("par_main", par_main_action);
  par_for = hpx_action_register("par_for", par_for_action);

  // run the main action
  return hpx_run(par_main, &args.n, sizeof(args.n));
}

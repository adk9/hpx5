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

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "hpx/future.h"

#define BUFFER_SIZE 128

/* command line options */
static bool         _text = false;            //!< send text data with the ping
static bool      _verbose = false;            //!< print to the terminal

/* actions */
static hpx_action_t _main = 0;
static hpx_action_t _ping = 0;
static hpx_action_t _pong = 0;

/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: pingponghpx [options] ITERATIONS\n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-m, send text in message\n"
          "\t-v, print verbose output \n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

static void _register_actions(void);

/** the pingpong message type */
typedef struct {
  int iterations;
  hpx_addr_t ping;
  hpx_addr_t pong;
} args_t;

/* utility macros */
#define CHECK_NOT_NULL(p, err)                                \
  do {                                                        \
    if (!p) {                                                 \
      fprintf(stderr, err);                                   \
      hpx_shutdown(1);                                        \
    }                                                         \
  } while (0)

#define RANK_PRINTF(format, ...)                                        \
  do {                                                                  \
    if (_verbose)                                                       \
      printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), \
             __VA_ARGS__);                                              \
  } while (0)

int main(int argc, char *argv[]) {
  hpx_config_t cfg = {
    .cores       = 0,
    .threads     = 0,
    .stack_bytes = 0,
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dmvh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'm':
      _text = true;
     case 'v':
      _verbose = true;
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
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

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    _usage(stderr);
    fprintf(stderr, "\nMissing iteration limit\n");
    return -1;
  }

  args_t args = {
    .iterations = strtol(argv[0], NULL, 10),
    .ping = hpx_lco_newfuture_new(BUFFER_SIZE),
    .pong = hpx_lco_newfuture_new(BUFFER_SIZE)
  };

  if (args.iterations == 0) {
    _usage(stderr);
    printf("read ITERATIONS as 0, exiting.\n");
    return -1;
  }

  printf("Running: {iterations: %d}, {message: %d}, {verbose: %d}\n",
         args.iterations, _text, _verbose);

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  _register_actions();

  const char *network = hpx_get_network_id();

  hpx_time_t start = hpx_time_now();
  e = hpx_run(_main, &args, sizeof(args));
  double elapsed = (double)hpx_time_elapsed_ms(start);
  double latency = elapsed / (args.iterations * 2);
  printf("average oneway latency (%s):   %f ms\n", network, latency);
  return e;
}

static int _action_main(args_t *args) {
  hpx_addr_t done = hpx_lco_and_new(2);

  hpx_call(HPX_HERE, _ping, args, sizeof(*args), done);
  hpx_addr_t there = HPX_HERE;
  if (hpx_get_num_ranks() > 1)
    there = HPX_THERE(1);
  hpx_call(there, _ping, args, sizeof(*args), done);

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  return HPX_SUCCESS;
}

/**
 * Send a ping message.
 */
static int _action_ping(args_t *args) {
  char msg_ping[BUFFER_SIZE];
  char msg_pong[BUFFER_SIZE];

  for (int i = 0; i < args->iterations; i++) {
    if (_text)
      snprintf(msg_ping, BUFFER_SIZE, "ping %d from (%d, %d)", i,
	       hpx_get_my_rank(), hpx_get_my_thread_id());
    
    RANK_PRINTF("pinging block %d, msg= '%s'\n", 1, msg_ping);
    
    hpx_lco_newfuture_setat(args->ping, 0, BUFFER_SIZE, msg_ping, HPX_NULL, HPX_NULL);
    hpx_lco_newfuture_getat(args->pong, 0, BUFFER_SIZE, msg_pong);

    RANK_PRINTF("Received pong msg= '%s'\n", msg_pong);
  }

  hpx_shutdown(HPX_SUCCESS);
}


/**
 * Handle a pong action.
 */
static int _action_pong(args_t *args) {
  char msg_ping[BUFFER_SIZE];
  char msg_pong[BUFFER_SIZE];

  for (int i = 0; i < args->iterations; i++) {
    hpx_lco_newfuture_getat(args->ping, 0, BUFFER_SIZE, msg_ping);

    if (_text)
      snprintf(msg_pong, BUFFER_SIZE, "pong %d from (%d, %d)", i,
	       hpx_get_my_rank(), hpx_get_my_thread_id());

    RANK_PRINTF("ponging block %d, msg= '%s'\n", 0, msg_pong);

    hpx_lco_newfuture_setat(args->pong, 0, BUFFER_SIZE, msg_pong, HPX_NULL, HPX_NULL);
  }

  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  _main = HPX_REGISTER_ACTION(_action_main);
  _ping = HPX_REGISTER_ACTION(_action_ping);
  _pong = HPX_REGISTER_ACTION(_action_pong);
}

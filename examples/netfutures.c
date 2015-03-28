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
static HPX_ACTION_DECL(_main);
static HPX_ACTION_DECL(_ping);
static HPX_ACTION_DECL(_pong);

/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: pingponghpx [options] ITERATIONS\n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-m, send text in message\n"
          "\t-v, print verbose output \n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

/** the pingpong message type */
typedef struct {
  int iterations;
  hpx_netfuture_t pingpong;
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

struct buffer {
  char data[BUFFER_SIZE];
};

int main(int argc, char *argv[]) {
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dmvh")) != -1) {
    switch (opt) {
     case 'c':
      break;
     case 't':
      break;
     case 'T':
      break;
     case 'm':
      _text = true;
      break;
     case 'v':
      _verbose = true;
      break;
     case 'D':
      break;
     case 'd':
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
  };

  if (args.iterations == 0) {
    _usage(stderr);
    printf("read ITERATIONS as 0, exiting.\n");
    return -1;
  }

  printf("Running: {iterations: %d}, {message: %d}, {verbose: %d}\n",
         args.iterations, _text, _verbose);

  hpx_time_t start = hpx_time_now();
  e = hpx_run(&_main, &args, sizeof(args));
  double elapsed = (double)hpx_time_elapsed_ms(start);
  double latency = elapsed / (args.iterations * 2);
  printf("average oneway latency:   %f ms\n", latency);
  return e;
}

static HPX_ACTION(_action_main, args_t *args) {
  printf("In main on rank %d\n", hpx_get_my_rank());
  hpx_status_t status =  hpx_netfutures_init(NULL);
  if (status != HPX_SUCCESS)
    return status;

  hpx_addr_t done = hpx_lco_and_new(2);

  hpx_netfuture_t base = hpx_lco_netfuture_new_all(2, BUFFER_SIZE);
  printf("Futures allocated\n");
  args->pingpong = base;

  hpx_call(HPX_HERE, _ping, done, args, sizeof(*args));
  hpx_call(HPX_THERE(1), _pong, done, args, sizeof(*args));

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_netfutures_fini();
  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Send a ping message.
 */
static HPX_ACTION(_ping, args_t *args) {
  printf("In ping on rank %d\n", hpx_get_my_rank());
  hpx_addr_t msg_ping_gas = hpx_gas_alloc_local(sizeof(struct buffer), sizeof(struct buffer));
  hpx_addr_t msg_pong_gas = hpx_gas_alloc_local(sizeof(struct buffer), sizeof(struct buffer));

  struct buffer *msg_ping;
  struct buffer *msg_pong;
  hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);

  for (int i = 0; i < args->iterations; i++) {
    if (_text)
      snprintf(msg_ping->data, BUFFER_SIZE, "ping %d from (%d, %d)", i,
           hpx_get_my_rank(), hpx_get_my_thread_id());

    RANK_PRINTF("pinging block %d, msg= '%s'\n", 1, msg_ping->data);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 1, BUFFER_SIZE, msg_ping_gas, lsync);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
    msg_pong_gas = hpx_lco_netfuture_getat(args->pingpong, 0, BUFFER_SIZE);
    hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);

    RANK_PRINTF("Received pong msg= '%s'\n", msg_pong->data);
    hpx_gas_unpin(msg_pong_gas);
    hpx_lco_netfuture_emptyat(args->pingpong, 0, HPX_NULL);
  }

  return HPX_SUCCESS;
}


/**
 * Handle a pong action.
 */
static HPX_ACTION(_pong, args_t *args) {
  printf("In pong on rank %d\n", hpx_get_my_rank());
  hpx_addr_t msg_ping_gas = hpx_gas_alloc_local(sizeof(struct buffer), sizeof(struct buffer));
  hpx_addr_t msg_pong_gas = hpx_gas_alloc_local(sizeof(struct buffer), sizeof(struct buffer));
  struct buffer *msg_ping;
  struct buffer *msg_pong;
  hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);


  for (int i = 0; i < args->iterations; i++) {
    msg_ping_gas = hpx_lco_netfuture_getat(args->pingpong, 1, BUFFER_SIZE);
    hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);

    if (_text)
      snprintf(msg_pong->data, BUFFER_SIZE, "pong %d from (%d, %d)", i,
           hpx_get_my_rank(), hpx_get_my_thread_id());

    RANK_PRINTF("ponging block %d, msg= '%s'\n", 0, msg_pong->data);

    hpx_gas_unpin(msg_ping_gas);
    hpx_lco_netfuture_emptyat(args->pingpong, 1, HPX_NULL);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 0, BUFFER_SIZE, msg_pong_gas, lsync);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
  }

  return HPX_SUCCESS;
}

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
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "hpx/future.h"

#define FIELD_WIDTH 20

/* actions */
static HPX_ACTION_DECL(_main);
static HPX_ACTION_DECL(_ping);
static HPX_ACTION_DECL(_pong);

#define MAX_MSG_SIZE 1024*1024

int skip = 0;
int loop = 10;
int iters = 1000;
int large_msg_size = 1024*1024;
int large_msg_iters = 100;

/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: pingponghpx \n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

/** the pingpong message type */
typedef struct {
  int msg_size;
  hpx_netfuture_t pingpong;
} args_t;

/* utility macros */
#define CHECK_NOT_NULL(p, err)                  \
  do {                                          \
  if (!p) {                                     \
  fprintf(stderr, err);                         \
  hpx_shutdown(1);                              \
  }                                             \
  } while (0)

#define RANK_PRINTF(format, ...)                \
  do {                                          \
  if (_verbose)                                 \
    printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), \
       __VA_ARGS__);                            \
  } while (0)

int main(int argc, char *argv[]) {

  return 77;

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  // FIXME: cfg.heap_bytes = (unsigned long)2*1024*1024*1024;

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  fprintf(stdout, "Starting the cost of Netfutures pingpong benchmark\n");
  fprintf(stdout, "%s%*s\n", "# MESG_SIZE ", FIELD_WIDTH, "LATENCY (ms)");

  return hpx_run(&_main, NULL, 0);
}

static HPX_ACTION(_main, void *args) {
  hpx_time_t start;
  hpx_netfuture_config_t cfg = HPX_NETFUTURE_CONFIG_DEFAULTS;
  cfg.max_array_number = ((int)ceil(log2(MAX_MSG_SIZE)) + 1) * (loop + skip);
  hpx_status_t status =  hpx_netfutures_init(&cfg);
  if (status != HPX_SUCCESS)
    return status;

  for (int k = 1; k <= MAX_MSG_SIZE; k = (k ? k * 2 : 1)) {
    fprintf(stdout, "%d\t", k);

    args_t args = {
      .msg_size = k,
    };

    for (int i = 0; i < loop + skip; i++) {
      if (i == skip) {
        start = hpx_time_now();
      }
      hpx_addr_t done = hpx_lco_and_new(2);

      hpx_netfuture_t base = hpx_lco_netfuture_new_all(2, k);
      args.pingpong = base;

      hpx_call(HPX_HERE, _ping, done, &args, sizeof(args));
      hpx_call(HPX_THERE(1), _pong, done, &args, sizeof(args));

      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
    }
    double elapsed = (double)hpx_time_elapsed_ms(start);
    double latency = elapsed / loop;
    fprintf(stdout, "%*f\n", FIELD_WIDTH, latency);
  }
  hpx_netfutures_fini();
  hpx_shutdown(HPX_SUCCESS);
}
/**
 * Send a ping message.
 */
static HPX_ACTION(_ping, args_t *args) {
  hpx_addr_t msg_ping_gas = hpx_gas_alloc_local(args->msg_size, 0);
  hpx_addr_t msg_pong_gas = hpx_gas_alloc_local(args->msg_size, 0);
  char *msg_ping;
  char *msg_pong;
  int myiters;
  hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);

  myiters = (args->msg_size >= large_msg_size)?large_msg_iters:iters;

  for (int i = 0; i < myiters; i++) {
    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 1, args->msg_size,
                            msg_ping_gas, lsync);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
    msg_pong_gas = hpx_lco_netfuture_getat(args->pingpong, 0, args->msg_size);
    hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);
    hpx_gas_unpin(msg_pong_gas);
    hpx_lco_netfuture_emptyat(args->pingpong, 0, HPX_NULL);

  }

  return HPX_SUCCESS;
}


/**
 * Handle a pong action.
 */
static HPX_ACTION(_pong, args_t *args) {
  hpx_addr_t msg_ping_gas = hpx_gas_alloc_local(args->msg_size, 0);
  hpx_addr_t msg_pong_gas = hpx_gas_alloc_local(args->msg_size, 0);
  char *msg_ping;
  char *msg_pong;
  int myiters;
  hpx_gas_try_pin(msg_pong_gas, (void**)&msg_pong);

  myiters = (args->msg_size >= large_msg_size)?large_msg_iters:iters;

  for (int i = 0; i < myiters; i++) {
    msg_ping_gas = hpx_lco_netfuture_getat(args->pingpong, 1, args->msg_size);
    hpx_gas_try_pin(msg_ping_gas, (void**)&msg_ping);
    hpx_gas_unpin(msg_ping_gas);
    hpx_lco_netfuture_emptyat(args->pingpong, 1, HPX_NULL);

    hpx_addr_t lsync = hpx_lco_future_new(0);
    hpx_lco_netfuture_setat(args->pingpong, 0, args->msg_size, msg_pong_gas,
                            lsync);
    hpx_lco_wait(lsync);
    hpx_lco_delete(lsync, HPX_NULL);
  }

  return HPX_SUCCESS;
}

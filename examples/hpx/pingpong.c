/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Pingong example
  examples/hpx/pingpong.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <hpx.h>

#define BUFFER_SIZE 128

/* command line arguments */
static int  arg_iter_limit = 1000;        /*!< the number of iterations */
static bool  arg_text_ping = false;       /*!< send text data with the ping */
static bool arg_screen_out = false;       /*!< print messages to the terminal */
static bool      arg_debug = false;       /*!< wait for a debugger to attach */

/* actions */
static hpx_action_t   ping = 0;
static hpx_action_t   pong = 0;

/* globals */
static int           count = 0;           //!< per-locality count of actions
static hpx_time_t    start = 0;           //!< keeps track of timing

/* helper functions */
static void print_usage(FILE *stream);
static void process_args(int argc, char *argv[]);
static void print_options(void);
static void wait_for_debugger(void);
static void register_actions(void);
static hpx_addr_t partner(void);

/** the pingpong message type */
typedef struct {
  int id;                                       /*!< the message ide  */
  char msg[BUFFER_SIZE];                        /*!< the text string (if -t) */
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
    if (arg_screen_out)                                                 \
      printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), __VA_ARGS__); \
  } while (0)

int main(int argc, char *argv[]) {
  wait_for_debugger();

  hpx_config_t config = {
    .scheduler_threads = 0,
    .stack_bytes = 0
  };

  if (hpx_init(&config)) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }
  process_args(argc, argv);
  print_options();
  register_actions();

  args_t args = {
    -1,
    {0}
  };
  return hpx_run(ping, &args, sizeof(args));
}

/**
 * Send a ping message.
 */
static int
action_ping(void *msg) {
  args_t *args = msg;

  // If this is the first occurrence of the ping operation, then grab the start
  // time, and output the fact that we're starting
  if (args->id < 0) {
    int rank = hpx_get_my_rank();
    int ranks = hpx_get_num_ranks();
    RANK_PRINTF("Running pingpong on %d ranks between rank %d and rank %d\n",
                ranks, rank, hpx_addr_to_rank(partner()));
    start = hpx_time_now();
  }

  // Get the id of the next message.
  int id = args->id + 1;

  // If we completed the number of ping-pong operations that we set out to do,
  // then output the latency and terminate execution.
  if (args->id >= arg_iter_limit) {
    hpx_time_t end = hpx_time_now();
    double elapsed = (double)hpx_time_to_ms(start - end);
    double latency = elapsed/(arg_iter_limit * 2);
    printf("average oneway latency (MPI):   %f ms\n", latency);
    hpx_shutdown(0);
  }

  // Otherwise we received a pong.
  RANK_PRINTF("received pong: '%s'\n", args->msg);

  // Generate a ping targeting pong.
  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
  CHECK_NOT_NULL(p, "Failed to acquire parcel in 'ping' action");
  hpx_parcel_set_action(p, pong);
  hpx_parcel_set_target(p, partner());
  args_t *a = hpx_parcel_get_data(p);
  a->id = id;
  if (arg_text_ping)
    snprintf(a->msg, BUFFER_SIZE, "ping %d from proc 0", id);

  RANK_PRINTF("sending ping to loc %d, count=%d, message='%s'\n",
              hpx_addr_to_rank(partner()), count, a->msg);

  hpx_parcel_send(p);
  ++count;
  return HPX_SUCCESS;
}

/**
 * Handle a pong action.
 */
static int
action_pong(void *msg) {
  args_t *args = msg;
  RANK_PRINTF("received '%s'\n", args->msg);
  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
  CHECK_NOT_NULL(p, "Could not allocate parcel in 'pong' action\n");
  hpx_parcel_set_action(p, ping);
  hpx_parcel_set_target(p, partner());
  args_t *a = hpx_parcel_get_data(p);
  a->id = args->id + 1;
  if (arg_text_ping) {
    char copy_buffer[BUFFER_SIZE];
    snprintf(copy_buffer, BUFFER_SIZE, "Received ping at %d: '%s'", args->id,
               args->msg);
    strncpy(a->msg, copy_buffer, BUFFER_SIZE);
  }

  RANK_PRINTF("sending pong to loc %d, count=%d, message='%s'\n",
              hpx_addr_to_rank(partner()), count, args->msg);
  hpx_parcel_send(p);
  ++count;
  return HPX_SUCCESS;
}

/**
 * Used for debugging. Causes a process to wait for a debugger to attach, and
 * set the value if i != 0.
 */
void
wait_for_debugger() {
  if (arg_debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(12);
  }
}

/**
 * Dump the usage string to the relevant file.
 */
void print_usage(FILE *stream) {
  fprintf(stream, "\n"
          "Usage: pingponghpx [-tvdh] <iterations>\n"
          "\t-t\tsend text in message\n"
          "\t-v\tprint verbose output \n"
          "\t-d\twait for debugger\n"
          "\t-h\tthis help display\n"
          "\n");
}

/**
 * Extract the arg_ values from the arguments strings.
 */
void
process_args(int argc, char *argv[]) {
  int opt = 0;
  while ((opt = getopt(argc, argv, "tvdh")) != -1) {
    switch (opt) {
    case 't':
      arg_text_ping = true;
      break;
    case 'v':
      arg_screen_out = true;
      break;
    case 'd':
      arg_debug = true;
      break;
    case 'h':
      print_usage(stdout);
      break;
    case '?':
    default:
      print_usage(stderr);
      exit(-1);
    }
  }

  argc -= optind;
  argv += optind;

  if (argc == 0) {
    fprintf(stderr, "Missing iteration limit\n");
    print_usage(stderr);
    exit(-1);
  }
  arg_iter_limit = strtol(argv[0], NULL, 10);
}

void
print_options(void) {
  if (hpx_get_my_rank() == 0)
    printf("Running with options: "
           "{iter limit: %d}, {text_ping: %d}, {screen_out: %d}\n",
           arg_iter_limit, arg_text_ping, arg_screen_out);
}

/**
 * Registers functions as actions.
 */
void
register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  ping = hpx_action_register("ping", action_ping);
  pong = hpx_action_register("pong", action_pong);
}

hpx_addr_t
partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return hpx_addr_from_rank((rank) ? 0 : ranks - 1);
}

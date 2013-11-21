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
#include <hpx.h>

#define BUFFER_SIZE 128

/* command line arguments */
static int arg_iter_limit  = 1000;        /*!< the number of iterations */
static bool arg_text_ping  = false;       /*!< send text data with the ping */
static bool arg_screen_out = false;       /*!< print messages to the terminal */
static bool arg_debug      = false;       /*!< wait for a debugger to attach */

/* actions */
static hpx_action_t pingpong = HPX_ACTION_NULL;
static hpx_action_t ping     = HPX_ACTION_NULL;
static hpx_action_t pong     = HPX_ACTION_NULL;
static hpx_action_t done     = HPX_ACTION_NULL;

/* globals */
static hpx_locality_t* other_loc = NULL;
static hpx_future_t done_fut     = {{0}};  /*!< main thread waits on this */
static int count                 = 0;      /*!< per-locality count of actions */

/* helper functions */
static void print_usage(FILE *stream);
static void process_args(int argc, char *argv[]);
static void wait_for_debugger();
static void init_other_loc();
static void register_actions();

/* utility macros */
#define CHECK_NOT_NULL(p, err)                                \
  do {                                                        \
    if (!p) {                                                 \
      fprintf(stderr, err);                                   \
      hpx_abort();                                            \
    }                                                         \
  } while (0)

int main(int argc, char *argv[]) {  
  if (hpx_init()) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  process_args(argc, argv);
  wait_for_debugger();
    /* I'm cheating by putting this before action registrations to make sure this
     gets done at all processes...

     LD: this seems like a persistent issue that we have, and is related to a
         SPMD programming mindset */
  init_other_loc();
  register_actions();
  
  hpx_timer_t ts;
  hpx_get_time(&ts);
  hpx_future_t *fut = NULL;
  hpx_action_invoke(pingpong, NULL, &fut);
  hpx_thread_wait(fut);
  double elapsed = hpx_elapsed_us(ts);
  double avg_oneway_latency = elapsed/((double)(arg_iter_limit*2));
  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);

  hpx_locality_destroy(other_loc);
  hpx_cleanup();

  return 0;
}

/** the pingpong message type */
typedef struct {
  int id;                                       /*!< the message ide  */
  char msg[BUFFER_SIZE];                        /*!< the text string (if -t) */
} args_t;

/**
 * The main action for each locality. Rank 0 initiates the pingpong operation,
 * and cleans up unused MPI processes.
 */
static void action_pingpong(void *unused) {  
  hpx_lco_future_init(&done_fut);

  unsigned int my_rank = hpx_get_my_locality()->rank;
  if (my_rank == 0) {
    printf("Running pingpong on %d ranks between rank 0 and rank %d\n",
           hpx_get_num_localities(),
           other_loc->rank); 
  
    /* ok to use the stack, since we're waiting on the done future */
    args_t args = { -1, {0}};
    hpx_action_invoke(ping, &args, NULL);
  }

  hpx_thread_wait(&done_fut);

  /* make sure any non-participating localities shut down */
  if (my_rank == 0) {
    for (int i = 1; i < hpx_get_num_localities() - 1; ++i) {
      if (arg_screen_out)
        printf("0: sending done to loc %d\n", i);
   
      hpx_parcel_t *p = hpx_parcel_acquire(0);
      CHECK_NOT_NULL(p, "Failed to acquire parcel in 'pingpong' action");
      hpx_parcel_set_action(p, done);
      hpx_parcel_send(hpx_locality_from_rank(i), p, NULL, NULL, NULL);
      hpx_parcel_release(p);
    }
  }
  
  hpx_thread_exit(0);
}

/**
 * Sets the done future.
 */
static void action_done(void* args) {
  if (arg_screen_out)
    printf("%d: recieved done\n", hpx_get_my_locality()->rank);

  hpx_lco_future_set_state(&done_fut);
  hpx_thread_exit(0);
}

/**
 * Send a ping message.
 */
static void action_ping(args_t* args) {
  if (arg_screen_out)
    printf("%d: received '%s'\n", hpx_get_my_locality()->rank, args->msg);

  int id = args->id + 1;

  if (id < arg_iter_limit) {
    args->id = id;
    if (arg_text_ping)
      snprintf(args->msg, BUFFER_SIZE, "ping %d from proc 0", args->id);

    if (arg_screen_out)
      printf("%d: sending ping to loc %d, count=%d, message='%s'\n",
             hpx_get_my_locality()->rank, other_loc->rank, count, args->msg);
    
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
    CHECK_NOT_NULL(p, "Failed to acquire parcel in 'ping' action");
    hpx_parcel_set_action(p, pong);
    hpx_parcel_set_data(p, args, sizeof(*args));
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
  }  
  else {
    hpx_parcel_t *p = hpx_parcel_acquire(0);
    CHECK_NOT_NULL(p, "Failed to acquire parcel in 'ping' action");
    hpx_parcel_set_action(p, done);
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
    hpx_action_invoke(done, NULL, NULL);
  }

  /* unsynchronized is ok because we only have one thread per locality */
  ++count;
  hpx_thread_exit(0);
}

/**
 * Send a pong argument.
 */
static void action_pong(args_t* args) {
  if (arg_screen_out)
    printf("%d: received '%s'\n", hpx_get_my_locality()->rank, args->msg);

  if (args->id < arg_iter_limit) {
    args->id = args->id + 1;

    if (arg_text_ping) {
      char copy_buffer[BUFFER_SIZE];
      snprintf(copy_buffer, BUFFER_SIZE,
               "Received ping at %d: '%s'", args->id, args->msg);
      strncpy(args->msg, copy_buffer, BUFFER_SIZE);
    }
    
    if (arg_screen_out)
      printf("%d: sending pong to loc %d, count=%d, message='%s'\n",
             hpx_get_my_locality()->rank, other_loc->rank, count, args->msg);

    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
    CHECK_NOT_NULL(p, "Could not allocate parcel in 'pong' action\n");
    hpx_parcel_set_action(p, ping);
    hpx_parcel_set_data(p, args, sizeof(*args));
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
  }

  count++;
  hpx_thread_exit(0);
}

/**
 * Used for debugging. Causes a process to wait for a debugger to attach, and
 * set the value if i != 0.
 */
void wait_for_debugger() {
  if (arg_debug) {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
      sleep(5);
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
void process_args(int argc, char *argv[]) {
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

  if (hpx_get_my_locality()->rank == 0)
    printf("Running with options: "
         "{iter limit: %d}, {text_ping: %d}, {screen_out: %d}\n",
         arg_iter_limit, arg_text_ping, arg_screen_out);
}

void init_other_loc(void) {
  unsigned int num_ranks = hpx_get_num_localities();
  unsigned int my_rank = hpx_get_my_locality()->rank;
  other_loc = hpx_locality_from_rank((my_rank == 0) ? num_ranks - 1 : 0);  
}

/**
 * Registers functions as actions.
 */
void register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  pingpong = hpx_action_register("pingpong", (hpx_func_t)action_pingpong);
  ping = hpx_action_register("ping", (hpx_func_t)action_ping);
  pong = hpx_action_register("pong", (hpx_func_t)action_pong);
  done = hpx_action_register("done", (hpx_func_t)action_done);
  hpx_action_registration_complete();
}

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include <pthread.h>

#define BENCHMARK "HPX COST OF LCO SEMAPHORES"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5

static int num[] = {
  1000000,
  2000000,
  3000000,
  4000000,
  5000000
};

/// This file tests cost of GAS operations
static void usage(FILE *stream) {
  fprintf(stream, "Usage:  [options]\n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

static hpx_action_t _main    = 0;
static hpx_action_t _thread1 = 0;
static hpx_action_t _thread2 = 0;

hpx_addr_t sem1, sem2;

static int _thread1_action(uint32_t *args) {
  uint32_t iter = *args;
  hpx_time_t t = hpx_time_now();
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem1);
    hpx_lco_sema_v(sem2);
  }
  fprintf(stdout, "Thread 1: %d%*g\n", iter, FIELD_WIDTH,
              hpx_time_elapsed_ms(t));

  return HPX_SUCCESS;
}

static int _thread2_action(uint32_t *args) {
  uint32_t iter = *args;
  hpx_time_t t = hpx_time_now();
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem2);
    hpx_lco_sema_v(sem1);
  }
  fprintf(stdout, "Thread 2: %d%*g\n", iter, FIELD_WIDTH,
              hpx_time_elapsed_ms(t));

  return HPX_SUCCESS;
}

static int _main_action(void *args) {
  hpx_time_t t;
  fprintf(stdout, HEADER);

  // Semaphore non contention test
  fprintf(stdout, "Semaphore non contention performance\n");
  fprintf(stdout, "%s%*s%*s\n", "# Iters " , FIELD_WIDTH, "Init time ",
          FIELD_WIDTH, " latency (ms)");
  for (int i = 0; i < sizeof(num)/sizeof(num[0]) ; i++) {
    fprintf(stdout, "%d", num[i]);
    t = hpx_time_now();
    hpx_addr_t mutex = hpx_lco_sema_new(num[i]);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));
    t = hpx_time_now();
    for (int j = 0; j < num[i]; j++) {
      hpx_lco_sema_p(mutex);
      hpx_lco_sema_v(mutex);
    }
    fprintf(stdout, "%*g\n", FIELD_WIDTH,  hpx_time_elapsed_ms(t));
  }

  fprintf(stdout, "\nSemaphore contention performance\n");
  fprintf(stdout, "%s%s%*s\n", "# Thread ID ", "Iters " , FIELD_WIDTH, "latency (ms)");
  // Semaphore contention test
  for (int i = 0; i < sizeof(num)/sizeof(num[0]) ; i++) {
    hpx_addr_t peers[] = {HPX_HERE, HPX_HERE};
    uint32_t value = num[i];
    int sizes[] = {sizeof(uint32_t), sizeof(uint32_t)};
    uint32_t array[] = {0, 0};
    void *addrs[] = {&array[0], &array[1]};

    hpx_addr_t futures[] = {
      hpx_lco_future_new(sizeof(uint32_t)),
      hpx_lco_future_new(sizeof(uint32_t))
    };

    sem1 = hpx_lco_sema_new(num[i]);
    sem2 = hpx_lco_sema_new(num[i]);

    hpx_call(peers[0], _thread1, &value, sizeof(uint32_t), futures[0]);
    hpx_call(peers[1], _thread2, &value, sizeof(uint32_t), futures[1]);

    hpx_lco_get_all(2, futures, sizes, addrs, NULL);

    hpx_lco_delete(futures[0], HPX_NULL);
    hpx_lco_delete(futures[1], HPX_NULL);
  }

  hpx_shutdown(HPX_SUCCESS);
}

int
main(int argc, char *argv[])
{

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  // Register the main action
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_thread1_action, &_thread1);
  HPX_REGISTER_ACTION(_thread2_action, &_thread2);

  // run the main action
  return hpx_run(&_main, NULL, 0);
}


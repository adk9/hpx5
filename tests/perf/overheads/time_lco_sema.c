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

static int _thread1_handler(uint32_t iter, hpx_addr_t sem1, hpx_addr_t sem2) {
  hpx_time_t t = hpx_time_now();
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem1);
    hpx_lco_sema_v(sem2);
  }
  fprintf(stdout, "Thread 1: %d%*g\n", iter, FIELD_WIDTH,
              hpx_time_elapsed_ms(t));

  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _thread1_handler, _thread1, HPX_UINT32, HPX_ADDR,
                      HPX_ADDR);

static int _thread2_handler(uint32_t iter, hpx_addr_t sem1, hpx_addr_t sem2) {
  hpx_time_t t = hpx_time_now();
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem2);
    hpx_lco_sema_v(sem1);
  }
  fprintf(stdout, "Thread 2: %d%*g\n", iter, FIELD_WIDTH,
          hpx_time_elapsed_ms(t));

  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _thread2_handler, _thread2, HPX_UINT32, HPX_ADDR,
                      HPX_ADDR);

static HPX_ACTION(_main, void) {
  fprintf(stdout, HEADER);
  fprintf(stdout, "Semaphore non contention performance\n");
  fprintf(stdout, "%s%*s%*s\n", "# Iters " , FIELD_WIDTH, "Init time ",
          FIELD_WIDTH, " latency (ms)");

  hpx_time_t t;
  for (int i = 0, e = sizeof(num)/sizeof(num[0]); i < e; ++i) {
    const int n = num[i];
    fprintf(stdout, "%d", n);
    t = hpx_time_now();
    hpx_addr_t mutex = hpx_lco_sema_new(n);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));
    t = hpx_time_now();
    for (int j = 0, e = n; j < e; ++j) {
      hpx_lco_sema_p(mutex);
      hpx_lco_sema_v(mutex);
    }
    hpx_lco_delete(mutex, HPX_NULL);
    fprintf(stdout, "%*g\n", FIELD_WIDTH,  hpx_time_elapsed_ms(t));
  }
  return HPX_SUCCESS;

  fprintf(stdout, "\nSemaphore contention performance\n");
  fprintf(stdout, "%s%s%*s\n", "# Thread ID ", "Iters " , FIELD_WIDTH, "latency (ms)");
  // Semaphore contention test
  for (int i = 0, e = sizeof(num)/sizeof(num[0]); i < e; ++i) {
    uint32_t value = num[i];
    hpx_addr_t peers[] = {
      HPX_HERE,
      HPX_HERE
    };

    int sizes[] = {
      sizeof(uint32_t),
      sizeof(uint32_t)
    };

    uint32_t array[] = {
      0,
      0
    };

    void *addrs[] = {
      &array[0],
      &array[1]
    };

    hpx_addr_t futures[] = {
      hpx_lco_future_new(sizeof(uint32_t)),
      hpx_lco_future_new(sizeof(uint32_t))
    };

    hpx_addr_t sems[2] = {
      hpx_lco_sema_new(value),
      hpx_lco_sema_new(value)
    };

    hpx_call(peers[0], _thread1, futures[0], &value, &sems[0], &sems[1]);
    hpx_call(peers[1], _thread2, futures[1], &value, &sems[0], &sems[1]);

    hpx_lco_get_all(2, futures, sizes, addrs, NULL);

    hpx_lco_delete(sems[1], HPX_NULL);
    hpx_lco_delete(sems[0], HPX_NULL);
    hpx_lco_delete(futures[1], HPX_NULL);
    hpx_lco_delete(futures[0], HPX_NULL);
  }

  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  // run the main action
  return hpx_run(&_main, NULL, 0);
}


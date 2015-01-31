#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include <pthread.h>
#include "libhpx/debug.h"

#define BENCHMARK "HPX COST OF LCO SEMAPHORES"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5

static int num[] = {
  100000,
  200000,
  300000,
  400000,
  500000
};

static int _thread1_handler(uint32_t iter, hpx_addr_t sem1, hpx_addr_t sem2) {
  dbg_assert(sem1 != sem2);
  dbg_assert(sem1 != hpx_thread_current_cont_target());
  dbg_assert(sem2 != hpx_thread_current_cont_target());
  hpx_time_t t = hpx_time_now();
  hpx_addr_t and = hpx_lco_and_new(iter);
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem1);
    hpx_lco_sema_v(sem2, and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  printf("Thread 1: %d%*g\n", iter, FIELD_WIDTH, hpx_time_elapsed_ms(t));
  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _thread1_handler, _thread1, HPX_UINT32, HPX_ADDR,
                      HPX_ADDR);

static int _thread2_handler(uint32_t iter, hpx_addr_t sem1, hpx_addr_t sem2) {
  dbg_assert(sem1 != sem2);
  dbg_assert(sem1 != hpx_thread_current_cont_target());
  dbg_assert(sem2 != hpx_thread_current_cont_target());

  hpx_time_t t = hpx_time_now();
  hpx_addr_t and = hpx_lco_and_new(iter);
  for (int j = 0; j < iter; j++) {
    hpx_lco_sema_p(sem2);
    hpx_lco_sema_v(sem1, and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  printf("Thread 2: %d%*g\n", iter, FIELD_WIDTH, hpx_time_elapsed_ms(t));
  return HPX_SUCCESS;
}

static HPX_ACTION_DEF(DEFAULT, _thread2_handler, _thread2, HPX_UINT32, HPX_ADDR,
                      HPX_ADDR);

static HPX_ACTION(_main, void) {
  printf(HEADER);
  printf("Semaphore non contention performance\n");
  printf("%s%*s%*s\n", "# Iters " , FIELD_WIDTH, "Init time ", FIELD_WIDTH,
         " latency (ms)");

  hpx_time_t t;
  for (int i = 0, e = sizeof(num)/sizeof(num[0]); i < e; ++i) {
    const int n = num[i];
    printf("%d", n);
    t = hpx_time_now();
    hpx_addr_t mutex = hpx_lco_sema_new(n);
    printf("%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));
    t = hpx_time_now();
    hpx_addr_t and = hpx_lco_and_new(n);
    for (int j = 0, e = n; j < e; ++j) {
      hpx_lco_sema_p(mutex);
      hpx_lco_sema_v(mutex, and);
    }
    hpx_lco_wait(and);
    hpx_lco_delete(and, HPX_NULL);
    hpx_lco_delete(mutex, HPX_NULL);
    printf("%*g\n", FIELD_WIDTH,  hpx_time_elapsed_ms(t));
  }

  printf("\nSemaphore contention performance\n");
  printf("%s%s%*s\n", "# Thread ID ", "Iters " , FIELD_WIDTH, "latency (ms)");
  // Semaphore contention test
  for (int i = 0, e = sizeof(num)/sizeof(num[0]); i < e; ++i) {
    int value = num[i];

    hpx_addr_t s1 = hpx_lco_sema_new(value);
    hpx_addr_t s2 = hpx_lco_sema_new(value);

    hpx_addr_t and = hpx_lco_and_new(2);
    hpx_call(HPX_HERE, _thread1, and, &value, &s1, &s2);
    hpx_call(HPX_HERE, _thread2, and, &value, &s1, &s2);
    hpx_lco_wait(and);

    hpx_lco_delete(and, HPX_NULL);
    hpx_lco_delete(s2, HPX_NULL);
    hpx_lco_delete(s1, HPX_NULL);
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


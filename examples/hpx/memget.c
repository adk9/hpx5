#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define BENCHMARK "OSU HPX MEMGET Test"

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "hpx/hpx.h"

#define MAX_MSG_SIZE         (1<<22)
#define SKIP_LARGE  10
#define LOOP_LARGE  100
#define LARGE_MESSAGE_SIZE  8192

int skip = 1000;
int loop = 10000;

#ifdef PACKAGE_VERSION
#   define HEADER "# " BENCHMARK " v" PACKAGE_VERSION "\n"
#else
#   define HEADER "# " BENCHMARK "\n"
#endif

#ifndef FIELD_WIDTH
#   define FIELD_WIDTH 20
#endif

#ifndef FLOAT_PRECISION
#   define FLOAT_PRECISION 2
#endif

void wtime(double *t)
{
  static int sec = -1;
  struct timeval tv;
  gettimeofday(&tv, (void *)0);
  if (sec < 0) sec = tv.tv_sec;
  *t = (tv.tv_sec - sec)*1.0e+6 + tv.tv_usec;
}

static hpx_action_t _main        = 0;
static hpx_action_t _init_array  = 0;
static hpx_action_t _memget      = 0;
static hpx_action_t _memget_pong = 0;

static int _init_array_action(size_t *args) {
  size_t n = *args;
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_addr_try_pin(target, (void**)&local))
    return HPX_RESEND;

  for(int i = 0; i < n; i++)
    local[i] = (hpx_get_my_rank() == 0) ? 'a' : 'b';
  hpx_thread_continue(sizeof(local), &local);
}


static int _memget_pong_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_addr_try_pin(target, (void**)&local))
    return HPX_RESEND;

  memcpy(args, local, hpx_thread_current_args_size());
  hpx_thread_exit(HPX_SUCCESS);
}

typedef struct {
  size_t n;
  hpx_addr_t addr;
  hpx_addr_t cont;
} _memget_args_t;


static int _memget_action(_memget_args_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  char *local;
  if (!hpx_addr_try_pin(target, (void**)&local))
    return HPX_RESEND;

  hpx_call(args->addr, _memget_pong, local, args->n, args->cont);
  hpx_thread_exit(HPX_SUCCESS);
}


static int _main_action(void *args) {
  double t_start = 0.0, t_end = 0.0;
  int rank = hpx_get_my_rank();
  int size = hpx_get_num_ranks();
  int peerid = (rank+1)%size;
  int i;

  if (size == 1 ) {
    fprintf(stderr, "This test requires at least two HPX threads\n");
    hpx_shutdown(0);
  }

  hpx_addr_t data = hpx_global_alloc(size, MAX_MSG_SIZE*2);
  hpx_addr_t remote = hpx_addr_add(data, MAX_MSG_SIZE*2 * peerid);

  fprintf(stdout, HEADER);
  fprintf(stdout, "# [ pairs: %d ]\n", size/2);
  fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
  fflush(stdout);

  hpx_addr_t lfut;
  hpx_addr_t rfut;
  for (size_t size = 1; size <= MAX_MSG_SIZE; size*=2) {
    char *local;
    lfut = hpx_lco_future_new(sizeof(void*));
    rfut = hpx_lco_future_new(sizeof(void*));

    hpx_call(remote, _init_array, &size, sizeof(size), rfut);
    hpx_call(data, _init_array, &size, sizeof(size), lfut);
    hpx_lco_get(lfut, &local, sizeof(local));
    hpx_lco_wait(rfut);

    if (size > LARGE_MESSAGE_SIZE) {
      loop = LOOP_LARGE;
      skip = SKIP_LARGE;
    }

    _memget_args_t args = {
      .n = size,
      .addr = data,
      .cont = HPX_NULL
    };

    for (i = 0; i < loop + skip; i++) {
      if(i == skip)
        wtime(&t_start);

      args.cont = hpx_lco_future_new(0);
      hpx_call(remote, _memget, &args, sizeof(args), HPX_NULL);
      hpx_lco_wait(args.cont);
      hpx_lco_delete(args.cont, HPX_NULL);
    }

    wtime(&t_end);
    hpx_lco_delete(lfut, HPX_NULL);
    hpx_lco_delete(rfut, HPX_NULL);

    double latency = (t_end - t_start)/(1.0 * loop);
    fprintf(stdout, "%-*lu%*.*f\n", 10, size, FIELD_WIDTH,
            FLOAT_PRECISION, latency);
    fflush(stdout);
  }
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

int main(int argc, char *argv[argc]) {
  hpx_config_t cfg = {
    .cores = 0,
    .threads = 0,
    .stack_bytes = 0
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
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
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int ranks = hpx_get_num_ranks();
  if (ranks < 2) {
    fprintf(stderr, "A minimum of 2 localities are required to run this test.\n");
    return -1;
  }

  _main        = HPX_REGISTER_ACTION(_main_action);
  _init_array  = HPX_REGISTER_ACTION(_init_array_action);
  _memget      = HPX_REGISTER_ACTION(_memget_action);
  _memget_pong = HPX_REGISTER_ACTION(_memget_pong_action);

  return hpx_run(_main, NULL, 0);
}

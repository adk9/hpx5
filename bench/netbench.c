#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hpx/hpx.h>

#define DEFAULT_ITERS 10000
hpx_action_t echo_pong;
hpx_action_t echo_finish;
unsigned long iterations;

typedef struct {
  hpx_addr_t lco;
  int src;
  int dst;
  size_t size;
  char data[];
} echo_args_t;

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: netbench [options] [ITERATIONS]\n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, show help\n");
}

void send_ping(hpx_addr_t lco, int src, int dst, size_t size) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, size);
  echo_args_t *echo_args = hpx_parcel_get_data(p);
  echo_args->lco = lco;
  echo_args->src = src;
  echo_args->dst = dst;
  echo_args->size = size;
  hpx_parcel_set_action(p, echo_pong);
  hpx_parcel_set_target(p, HPX_THERE(dst));
  // here we use an asynchronous parcel send since we don't care about local completion;
  // the runtime gave us the buffer with the acquire, and we gave it back;
  // and it won't affect our timing because the local send completion isn't relevant
  hpx_parcel_send(p, HPX_NULL);
}

void send_pong(echo_args_t *args) {
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, args->size);
  echo_args_t *return_args = hpx_parcel_get_data(p);
  // unfortunately, can't avoid some at least some copying
  // but we are only copying sizeof(echo_args_t) since this isn't a memory performance test
  memcpy(return_args, args, sizeof(echo_args_t));
  hpx_parcel_set_action(p, echo_finish);
  hpx_parcel_set_target(p, HPX_THERE(args->src));
  // here we use an asynchronous parcel send since we don't care about local completion;
  // the runtime gave us the buffer with the acquire, and we gave it back;
  // and it won't affect our timing because the local send completion isn't relevant
  hpx_parcel_send(p, HPX_NULL);
}

int echo_pong_action(echo_args_t *args) {
  if (args->dst != hpx_get_my_rank())
    hpx_shutdown(-1);
  send_pong(args);
  return HPX_SUCCESS;
}

int echo_finish_action(echo_args_t *args) {
  //hpx_lco_gencount_inc(args->lco, HPX_NULL);
  hpx_lco_and_set(args->lco, HPX_NULL);
  return HPX_SUCCESS;
}

int hpx_main_action(void *args) {
  if (hpx_get_num_ranks() < 2) {
    printf("Too few ranks, need at least two.\n");
    return HPX_ERROR;
  }
    
  size_t sizes[] = {1, 128, 1024, 4096, 8192, 64*1024, 256*1024, 1024*1024, 4*1024*1024};
  int num_sizes = sizeof(sizes)/sizeof(size_t);
  //  hpx_addr_t lco = hpx_lco_gencount_new(num_sizes);
  hpx_addr_t lco;
  printf("size\tS_time_ms\tavg_time_ms\tmsgs/s\tbytes/s\n");
  for (int i = 0; i < num_sizes; i++) {
    size_t actual_size = sizes[i];
    if (sizeof(echo_args_t) > sizes[i])
      actual_size = sizeof(echo_args_t);

    lco = hpx_lco_and_new(iterations);
    //lco = hpx_lco_gencount_new(iterations);
    hpx_time_t time_start = hpx_time_now();
    for (int j = 0; j < iterations; j++)
      send_ping(lco, 0, 1, actual_size);   
    // hpx_lco_gencount_wait(lco, iterations);
    hpx_lco_wait(lco);
    hpx_time_t time_end = hpx_time_now();
    double time_in_ms = hpx_time_diff_ms(time_start, time_end);
    double avg_time_in_ms = time_in_ms/iterations;
    printf("%zu\t%.4g\t%.4g\t%.6g\t%.4g\n", 
	   actual_size, 
	   time_in_ms,
	   avg_time_in_ms,
	   (iterations*1.0)/time_in_ms*1000.0,
	   (iterations*1.0)/time_in_ms*1000.0*actual_size);
    hpx_lco_delete(lco, HPX_NULL);
  }

  hpx_shutdown(0);
  //return HPX_SUCCESS;
}



int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dmvh")) != -1) {
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

  iterations = 0;
  if (argc != 0)
    iterations = strtol(argv[0], NULL, 10);
  
  if (iterations == 0) {
    iterations = DEFAULT_ITERS;
    printf("read ITERATIONS as 0, setting them to default of %lu.\n", iterations);
  }

  hpx_init(&cfg);

  echo_pong = HPX_REGISTER_ACTION(echo_pong_action);
  echo_finish = HPX_REGISTER_ACTION(echo_finish_action);
  hpx_action_t hpx_main = HPX_REGISTER_ACTION(hpx_main_action);

  int e = hpx_run(hpx_main, NULL, 0);
  return e;
}

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "common.h"

#define BENCHMARK "HPX COST OF LCO AND"

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

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: andlco overhead \n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, set logging level\n"
          "\t-s, set stack size\n"
          "\t-p, set per-PE global heap size\n"
          "\t-r, set send/receive request limit\n"
          "\t-h, this help display\n");
}

static hpx_action_t _lco_set  = 0;
static hpx_action_t _main = 0;
static hpx_action_t _empty = 0;


static int _lco_set_action(void *args) {
  hpx_lco_and_set(*(hpx_addr_t*)args, HPX_NULL);
  return HPX_SUCCESS;
}

static int _empty_action(hpx_addr_t *args) {
  return HPX_SUCCESS;
}

static int _main_action(int *args) {
  hpx_time_t t, t1;
  fprintf(test_log, HEADER);
  fprintf(test_log, "# Latency in (ms)\n");
  fprintf(test_log, "%s%*s%*s%*s\n", "# Iters " , FIELD_WIDTH, "Init time ",
          FIELD_WIDTH, "LCO Set", FIELD_WIDTH, "Delete");

  for (int i = 0; i < sizeof(num)/sizeof(num[0]) ; i++) {
    fprintf(test_log, "%d", num[i]);

    hpx_addr_t done = hpx_lco_future_new(num[i]);

    t = hpx_time_now();
    hpx_addr_t setlco = hpx_lco_and_new(num[i]);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    // Time it take to set empty action
    hpx_addr_t completed = hpx_lco_and_new(num[i]);
    t1 = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(HPX_HERE, _empty, &setlco, sizeof(setlco), completed);
    hpx_lco_wait(completed);
    double empty_t = hpx_time_elapsed_ms(t1);
    hpx_lco_delete(completed, HPX_NULL);

    // Time to set for LCO argument
    t = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(HPX_HERE, _lco_set, &setlco, sizeof(setlco), done);
    hpx_lco_wait(setlco);
    double end_t = hpx_time_elapsed_ms(t);
    fprintf(test_log, "%*g",FIELD_WIDTH, end_t - empty_t);

    t = hpx_time_now();
    hpx_lco_delete(setlco, HPX_NULL);
    fprintf(test_log, "%*g\n",FIELD_WIDTH, hpx_time_elapsed_ms(t));

    hpx_lco_delete(done, HPX_NULL);
  }

  fclose(test_log);
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dl:s:p:r:q:h")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'T':
      cfg.transport = atoi(optarg);
      assert(0 <= cfg.transport && cfg.transport < HPX_TRANSPORT_MAX);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'l':
      cfg.log_level = atoi(optarg);
      break;
     case 's':
      cfg.stack_bytes = strtoul(optarg, NULL, 0);
      break;
     case 'p':
      cfg.heap_bytes = strtoul(optarg, NULL, 0);
      break;
     case 'r':
      cfg.req_limit = strtoul(optarg, NULL, 0);
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

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  test_log = fopen("test.log", "a+");
  fprintf(test_log, "\n");

  // register the actions
  _lco_set  = HPX_REGISTER_ACTION(_lco_set_action);
  _main = HPX_REGISTER_ACTION(_main_action);
  _empty = HPX_REGISTER_ACTION(_empty_action);

  // run the main action
  return hpx_run(_main, NULL, 0);
}

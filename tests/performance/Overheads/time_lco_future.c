#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "common.h"

#define BENCHMARK "HPX COST OF LCO Future"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: future lco overhead \n"
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

static hpx_action_t _main = 0;
static hpx_action_t set_value = 0;
static hpx_action_t get_value = 0;

#define T int

static T value;

static int num_readers[]  ={
  1,
  4,
  8,
  32,
  64,
 128,
 192
};

static int action_get_value(void *args) {
  HPX_THREAD_CONTINUE(value);
}

static int action_set_value(void *args) {
  value = *(T*)args;
  return HPX_SUCCESS;
}

static int _main_action(int *args) {
  hpx_time_t t;
  int count;

  fprintf(test_log, HEADER);
  fprintf(test_log, "# Latency in (ms)\n");

  t = hpx_time_now();
  hpx_addr_t done = hpx_lco_future_new(0);
  fprintf(test_log, "Creation time: %g\n", hpx_time_elapsed_ms(t));

  value = 1234;

  t = hpx_time_now();
  hpx_call(HPX_HERE, set_value, &value, sizeof(value), done);
  fprintf(test_log, "Value set time: %g\n", hpx_time_elapsed_ms(t));

  t = hpx_time_now();
  hpx_lco_wait(done);
  fprintf(test_log, "Wait time: %g\n", hpx_time_elapsed_ms(t));

  t = hpx_time_now();
  hpx_lco_delete(done, HPX_NULL);
  fprintf(test_log, "Deletion time: %g\n", hpx_time_elapsed_ms(t));

  fprintf(test_log, "%s\t%*s%*s%*s\n", "# NumReaders " , FIELD_WIDTH,
         "Get_Value ", FIELD_WIDTH, " LCO_Getall ", FIELD_WIDTH, "Delete");

  for (int i = 0; i < sizeof(num_readers)/sizeof(num_readers[0]); i++) {
    fprintf(test_log, "%d\t\t", num_readers[i]);
    count = num_readers[i];
    int values[count];
    void *addrs[count];
    int sizes[count];
    hpx_addr_t futures[count];

    for (int j = 0; j < count; j++) {
      addrs[j] = &values[j];
      sizes[j] = sizeof(int);
      futures[j] = hpx_lco_future_new(sizeof(int));
    }

    t = hpx_time_now();
    for (int j = 0; j < count; j++) {
      t = hpx_time_now();
      hpx_call(HPX_HERE, get_value, NULL, 0, futures[j]);
      hpx_lco_wait(futures[j]);
    }
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    hpx_lco_get_all(count, futures, sizes, addrs, NULL);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    for (int j = 0; j < count; j++)
      hpx_lco_delete(futures[j], HPX_NULL);
    fprintf(test_log, "%*g\n", FIELD_WIDTH, hpx_time_elapsed_ms(t));
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
  _main = HPX_REGISTER_ACTION(_main_action);
  set_value = hpx_register_action("set_value", action_set_value);
  get_value = hpx_register_action("get_value", action_get_value);

  // run the main action
  return hpx_run(_main, NULL, 0);
}

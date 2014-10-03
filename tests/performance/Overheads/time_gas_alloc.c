#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "common.h"

#define MAX_BYTES        1073741824 //1gb
#define SKIP_LARGE       10
#define LOOP_LARGE       100

int skip = 1000;
int loop = 10000;

#define BENCHMARK "HPX COST OF GAS ALLOCATIONS (ms)"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5
/// This file tests cost of GAS operations
static void usage(FILE *stream) {
  fprintf(stream, "Usage:  [options]\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-T, select a transport by number (see hpx_config.h)\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-l, logging level\n"
          "\t-h, this help display\n");
}

static hpx_action_t _main    = 0;

static int _main_action(void *args) {
  hpx_addr_t local, global, calloc_global;
  hpx_time_t t;

  fprintf(test_log, HEADER);
  fprintf(test_log, "%s\t%*s%*s%*s%*s%*s%*s\n", "# Size ", HEADER_FIELD_WIDTH,
           " LOCAL_ALLOC ", HEADER_FIELD_WIDTH, " FREE ", HEADER_FIELD_WIDTH, 
           " GLOBAL_ALLOC ", HEADER_FIELD_WIDTH, " FREE ", HEADER_FIELD_WIDTH,
           " GLOBAL_CALLOC ", HEADER_FIELD_WIDTH, " FREE ");

  for (size_t size = 1; size <= MAX_BYTES; size*=2) {
    t = hpx_time_now();
    local = hpx_gas_alloc(size);
    fprintf(test_log, "%-*lu%*g", 10,  size, FIELD_WIDTH, hpx_time_elapsed_ms(t));
   
    t = hpx_time_now();
    hpx_gas_free(local, HPX_NULL);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    global = hpx_gas_global_alloc(hpx_get_num_ranks(), size);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));   

    t = hpx_time_now();
    hpx_gas_free(global, HPX_NULL);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    calloc_global = hpx_gas_global_calloc(hpx_get_num_ranks(), size);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    hpx_gas_free(calloc_global, HPX_NULL);
    fprintf(test_log, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));
    fprintf(test_log, "\n");
  }
  fclose(test_log);
  hpx_shutdown(HPX_SUCCESS);
}

int
main(int argc, char *argv[])
{
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:T:d:Dl:h")) != -1) {
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
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  test_log = fopen("test.log", "a+");
  fprintf(test_log, "Starting the cost of GAS Allocation benchmark\n");

  // Register the main action
  _main    = HPX_REGISTER_ACTION(_main_action);

  // run the main action
  return hpx_run(_main, NULL, 0);
}

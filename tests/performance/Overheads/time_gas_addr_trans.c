#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "common.h"

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: address translation perf test [Options] num_threads\n"
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

static hpx_action_t _address_translation = 0;
static hpx_action_t _main = 0;

#define BENCHMARK "HPX COST OF GAS ADDRESS TRANSLATION (ms)"

#define HEADER "# " BENCHMARK "\n"

#define TEST_BUF_SIZE (1024*64) //64k
#define FIELD_WIDTH 20

static int num[] = {
  1000000,
  2000000,
  3000000,
  4000000,
  5000000
};

static int _address_translation_action(void* args) {
  hpx_addr_t local = hpx_thread_current_target();

  // The pinned local address
  int *data = NULL;

  // Performs address translation. This will try to perform a global-to-local
  // translation on the global addr, and set local to the local address if it
  // it is successful.
  if (!hpx_gas_try_pin(local, (void **)&data))
    return HPX_RESEND;

  // make sure to unpin the address
  hpx_gas_unpin(local);

  hpx_thread_continue(0, NULL);
}

static int _main_action(void *args) {
  hpx_time_t now;
  double elapsed;
  int size = HPX_LOCALITIES;
  int ranks = hpx_get_num_ranks();
  uint32_t blocks = size;

  fprintf(test_log, HEADER);
  fprintf(test_log, "localities: %d, ranks and blocks per rank = %d, %d\n",
                  size, ranks, blocks/ranks);
  fprintf(test_log, "%s%*s%*s%*s\n", "# Num threads ", FIELD_WIDTH,
          "GAS ALLOC", FIELD_WIDTH, "GLOBAL_ALLOC", FIELD_WIDTH,
          "GLOBAL_CALLOC");

  for (int i = 0; i < sizeof(num)/sizeof(num[0]); i++) {
    fprintf(test_log, "%d", num[i]);

    hpx_addr_t local = hpx_gas_alloc(TEST_BUF_SIZE);
    hpx_addr_t completed = hpx_lco_and_new(num[i]);
    now = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(local, _address_translation, 0 , 0, completed);
    elapsed = hpx_time_elapsed_ms(now)/1e3;
    hpx_lco_wait(completed);
    fprintf(test_log, "%*.7f", FIELD_WIDTH,  elapsed);
    hpx_lco_delete(completed, HPX_NULL);
    hpx_gas_free(local, HPX_NULL);

    hpx_addr_t global = hpx_gas_global_alloc(blocks, TEST_BUF_SIZE);
    hpx_addr_t done = hpx_lco_and_new(num[i]);
    now = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(global, _address_translation, 0 , 0, done);
    elapsed = hpx_time_elapsed_ms(now)/1e3;
    hpx_lco_wait(done);
    fprintf(test_log, "%*.7f", FIELD_WIDTH,  elapsed);
    hpx_lco_delete(done, HPX_NULL);
    hpx_gas_free(global, HPX_NULL);

    hpx_addr_t callocMem = hpx_gas_global_calloc(blocks, TEST_BUF_SIZE);
    hpx_addr_t and = hpx_lco_and_new(num[i]);
    now = hpx_time_now();
    for (int j = 0; j < num[i]; j++)
      hpx_call(callocMem, _address_translation, 0 , 0, and);
    elapsed = hpx_time_elapsed_ms(now)/1e3;
    hpx_lco_wait(and);
    fprintf(test_log, "%*.7f", FIELD_WIDTH,  elapsed);
    hpx_lco_delete(and, HPX_NULL);
    hpx_gas_free(callocMem, HPX_NULL);

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

  argc -= optind;
  argv += optind;

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  test_log = fopen("test.log", "a+");
  // register the actions
  _address_translation     = HPX_REGISTER_ACTION(_address_translation_action);
  _main    = HPX_REGISTER_ACTION(_main_action);

  // run the main action
  return hpx_run(_main, NULL, 0);
}

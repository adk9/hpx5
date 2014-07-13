#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

/// @file parse.c
/// @brief Parse the example command line arguments into an hpx_config_t
///        structure and a main_args_t structure.


static void
_usage(FILE *f, int error) {
  fprintf(f, "Usage: ./example [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-s, stack size in bytes\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
  fflush(f);
  exit(error);
}

void
parse_command_line(int argc, char * const argv[argc],
                   hpx_config_t *config, main_args_t *args)
{
  assert(config);
  assert(args);

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:s:d:D:n:i:h")) != -1) {
    switch (opt) {
     case 'c':
      args->cores = config->cores = atoi(optarg);
      break;
     case 't':
      config->threads = atoi(optarg);
      break;
     case 's':
      config->stack_bytes = atoi(optarg);
      break;
     case 'D':
      config->wait = HPX_WAIT;
      config->wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      config->wait = HPX_WAIT;
      config->wait_at = atoi(optarg);
      break;
     case 'n':
      args->nDoms = atoi(optarg);
      break;
     case 'i':
      args->maxCycles = atoi(optarg);
      break;
     case 'h':
      _usage(stdout, 0);
     case '?':
     default:
      _usage(stderr, -1);
    }
  }
}

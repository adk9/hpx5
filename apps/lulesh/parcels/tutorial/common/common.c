#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

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

int
main(int argc, char * const argv[argc])
{
  // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 10,
    .cores = 8
  };

  // allocate the default HPX configuration on the stack
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  cfg.cores = args.cores;
  cfg.threads = args.cores;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:s:d:D:n:i:h")) != -1) {
    switch (opt) {
     case 'c':
      args.cores = cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 's':
      cfg.stack_bytes = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'n':
      args.nDoms = atoi(optarg);
      break;
     case 'i':
      args.maxCycles = atoi(optarg);
      break;
     case 'h':
      _usage(stdout, 0);
     case '?':
     default:
      _usage(stderr, -1);
    }
  }

  // initialize HPX
  int err = hpx_init(&cfg);
  if (err)
    return err;

  // register HPX actions
  tutorial_init_actions();

  // register the main action
  hpx_action_t _main = HPX_REGISTER_ACTION(tutorial_main_action);

  // run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}

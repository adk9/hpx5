#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"

static void
_usage(FILE *f, int error) {
  fprintf(f, "Usage: ./example [options]\n"
          "\t-n, number of domains\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
  hpx_print_help();
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
  };

  // initialize HPX
  int err = hpx_init(&argc, &argv);
  if (err)
    return err;

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "n:i:h?")) != -1) {
    switch (opt) {
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

  // register HPX actions
  tutorial_init_actions();

  // register the main action
  hpx_action_t _main = HPX_REGISTER_ACTION(tutorial_main_action);

  // run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}

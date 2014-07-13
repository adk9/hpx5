#include "common.h"

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
  hpx_config_t cfg = {
    .cores = args.cores,
    .threads = args.cores,
    .stack_bytes = 0,
    .gas = HPX_GAS_PGAS
  };

  // parse the command line
  parse_command_line(argc, argv, &cfg, &args);

  // initialize HPX
  int err = hpx_init(&cfg);
  if (err)
    return err;

  // register HPX actions
  hpx_action_t _main = HPX_REGISTER_ACTION(main_action);

  // run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}

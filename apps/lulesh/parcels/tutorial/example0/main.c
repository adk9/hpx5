#include "common.h"

int
tutorial_main_action(const main_args_t *args)
{
  // output the arguments we're running with
  printf("Number of domains: %d maxCycles: %d cores: %d\n",
         args->nDoms, args->maxCycles, args->cores);
  fflush(stdout);

  // and shutdown (arg is error code, if necessary)
  hpx_shutdown(0);
}

void
tutorial_init_actions(void)
{
}

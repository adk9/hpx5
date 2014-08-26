#include "common.h"
#include "domain.h"

int
tutorial_main_action(const main_args_t *args)
{
  // allocate and start a timer
  hpx_time_t t1 = hpx_time_now();

  // output the arguments we're running with
  printf("Number of domains: %d maxCycles: %d cores: %d\n",
         args->nDoms, args->maxCycles, args->cores);
  fflush(stdout);

  int tp = (int) (cbrt(args->nDoms) + 0.5);
  if (tp*tp*tp != args->nDoms) {
    fprintf(stderr, "nDoms must be a cube of an integer (1, 8, 27, ...)\n");
    goto shutdown;
  }

  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));
  hpx_gas_free(domain);

 shutdown:
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_shutdown(0);
}

void
tutorial_init_actions(void)
{
}

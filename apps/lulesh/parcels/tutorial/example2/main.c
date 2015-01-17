#include "common.h"
#include "domain.h"

static hpx_action_t _initDomain = 0;


/// Initialize a domain.
static int
_initDomain_action(const InitArgs *args)
{
  // Get the address this parcel was sent to, and map it to a local address---if
  // this fails then the message arrived at the wrong place due to AGAS
  // movement, so resend the parcel.
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // Update the domain with the argument data.
  ld->rank = args->index;
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;

  // make sure to unpin the domain, so that AGAS can move it if it wants to
  hpx_gas_unpin(local);

  printf("Initialized domain %u\n", args->index);

  // return success---this triggers whatever continuation was set by the parcel
  // sender
  return HPX_SUCCESS;
}


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

  // Allocate the domain array
  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));

  // Allocate an and gate that we can wait on to detect that all of the domains
  // have completed initialization.
  hpx_addr_t done = hpx_lco_and_new(args->nDoms);

  // Send the initDomain action to all of the domains, in parallel.
  for (int i = 0, e = args->nDoms; i < e; ++i) {

    // hpx_call() will copy this
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .cores = args->cores
    };

    // compute the offset for this domain
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);

    // and send the initDomain action, with the done LCO as the continuation
    hpx_call(block, _initDomain, &init, sizeof(init), done);
  }

  // wait for initialization
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // and free the domain
  hpx_gas_free(domain);

 shutdown:
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_shutdown(0);
}


/// Register the actions that we need.
void
tutorial_init_actions(void)
{
  HPX_REGISTER_ACTION(_initDomain_action, &_initDomain);
}

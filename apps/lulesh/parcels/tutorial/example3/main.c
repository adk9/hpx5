#include "common.h"
#include "domain.h"


static hpx_action_t _initDomain = 0;
static hpx_action_t _advanceDomain = 0;


static int
_initDomain_action(const InitArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = args->index;
  ld->maxcycles = args->maxcycles;
  ld->nDoms = args->nDoms;

  // Record the AND gate LCO to trigger when execution terminates
  ld->complete = args->complete;

  hpx_gas_unpin(local);

  printf("Initialized domain %u\n", args->index);

  return HPX_SUCCESS;
}


/// Advance a domain one timestep.
static int
_advanceDomain_action(const unsigned long *epoch)
{
  // Get the address this parcel was sent to, and map it to a local address---if
  // this fails then the message arrived at the wrong place due to AGAS
  // movement, so resend the parcel.
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  // signal completion---no data is required for this, nor do we care about
  // waiting for local or remote completion
  hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
  printf("Finished processing domain %u\n", domain->rank);

  // unpin the domain
  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}



int
tutorial_main_action(const main_args_t *args)
{
  hpx_time_t t1 = hpx_time_now();

  printf("Number of domains: %d maxCycles: %d cores: %d\n",
         args->nDoms, args->maxCycles, args->cores);
  fflush(stdout);

  int tp = (int) (cbrt(args->nDoms) + 0.5);
  if (tp*tp*tp != args->nDoms) {
    fprintf(stderr, "nDoms must be a cube of an integer (1, 8, 27, ...)\n");
    goto shutdown;
  }

  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(args->nDoms);

  // allocate a complete AND gate LCO
  hpx_addr_t complete = hpx_lco_and_new(args->nDoms);

  for (int i = 0, e = args->nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .cores = args->cores,
      .complete = complete                // send along the complete LCO address
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _initDomain, &init, sizeof(init), done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  // start processing the domains, and wait on 'complete' for them all to finish
  const unsigned long epoch = 0;
  for (int i = 0, e = args->nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

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
  HPX_REGISTER_ACTION(_advanceDomain_action, &_advanceDomain);
}

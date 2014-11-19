#include "common.h"
#include "domain.h"

static hpx_action_t _initDomain = 0;


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

  hpx_gas_unpin(local);
  printf("Initialized domain %u\n", args->index);
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

  for (int i = 0, e = args->nDoms; i < e; ++i) {
    // Allocate a parcel with enough inline buffer space to store an InitArgs
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(InitArgs));

    // Get access to the parcel's buffer, and fill it with the necessary data.
    InitArgs *init = hpx_parcel_get_data(p);
    init->index = i;
    init->nDoms = args->nDoms;
    init->maxcycles = args->maxCycles;
    init->cores = args->cores;

    // set the target address and action for the parcel
    hpx_parcel_set_target(p, hpx_addr_add(domain, sizeof(Domain) * i));
    hpx_parcel_set_action(p, _initDomain);

    // set the continuation target and action for the parcel
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // and send the parcel---we used the parcel's buffer directly so we don't
    // have to wait for local completion (hence HPX_NULL), also this transfers
    // ownership of the parcel back to the runtime so we don't need to release
    // it
    hpx_parcel_send(p, HPX_NULL);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
  hpx_gas_free(domain);

 shutdown:
  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_shutdown(0);
}


void
tutorial_init_actions(void)
{
  HPX_REGISTER_ACTION(&_initDomain, _initDomain_action);
}

#include "common.h"
#include "domain.h"


static hpx_action_t _initDomain = 0;
static hpx_action_t _advanceDomain = 0;


/// Initialize a double to a large value.
static void
_initDouble(double *input)
{
  *input = 99999999.0;
}


/// Update *lhs with with the min(lhs, rhs);
static void
_minDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs < *rhs) ? *lhs : *rhs;
}


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
  ld->complete = args->complete;
  ld->cycle = 0;

  // record the newdt allreduce
  ld->newdt = args->newdt;

  hpx_gas_unpin(local);

  printf("Initialized domain %u\n", args->index);

  return HPX_SUCCESS;
}


static int
_advanceDomain_action(const unsigned long *epoch)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if (domain->maxcycles <= domain->cycle) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    printf("Finished processing %lu epochs at domain %u\n", *epoch, domain->rank);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the allreduce
  double gnewdt = 3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_set(domain->newdt, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);

  // do useful work here at some point (overlapped with ALLREDUCE)

  // Get the reduced value, and print the debugging string.
  double newdt;
  hpx_lco_get(domain->newdt, sizeof(double), &newdt);
  printf(" TEST cycle %d rank %d newdt %g\n", domain->cycle, domain->rank,
         newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
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
  hpx_addr_t complete = hpx_lco_and_new(args->nDoms);

  // allocate the allreduce, using _minDouble as the reduction and _initDouble
  // as the initialization
  hpx_addr_t newdt = hpx_lco_allreduce_new(args->nDoms, sizeof(double),
                                           (hpx_commutative_associative_op_t)_minDouble,
                                           (void (*)(void *))_initDouble);

  for (int i = 0, e = args->nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .cores = args->cores,
      .complete = complete,
      .newdt = newdt                  // pass along the global allreduce address
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _initDomain, &init, sizeof(init), done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  const unsigned long epoch = 0;
  for (int i = 0, e = args->nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);
  // hpx_lco_delete(newdt, HPX_NULL); HPX BUG
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

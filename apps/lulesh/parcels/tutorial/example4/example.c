#include "example.h"
#include "domain.h"

static hpx_action_t _main          = 0;
static hpx_action_t _initDomain    = 0;
static hpx_action_t _advanceDomain = 0;

static int _initDomain_action(InitArgs *init) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = init->index;
  ld->maxcycles = init->maxcycles;
  ld->nDoms = init->nDoms;
  ld->complete = init->complete;
  ld->cycle = 0;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _advanceDomain_action(unsigned long *epoch) {
  const unsigned long n = *epoch;
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  if ( domain->cycle >= domain->maxcycles ) {
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  printf(" TEST cycle %d rank %d\n",domain->cycle,domain->rank);

  domain->cycle++;
  hpx_gas_unpin(local);
  unsigned long next = n + 1;
  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
}

static int _main_action(int *input)
{
  hpx_time_t t1 = hpx_time_now();
 
  int nDoms, maxcycles, cores, tp,i,j,k;
  nDoms = input[0];
  maxcycles = input[1];
  cores = input[2];

  tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  hpx_addr_t domain = hpx_gas_global_alloc(nDoms,sizeof(Domain));

  hpx_addr_t init = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  for (k=0;k<nDoms;k++) {
    InitArgs args = {
      .index = k,
      .nDoms = nDoms,
      .maxcycles = maxcycles,
      .cores = cores,
      .complete = complete
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k);
    hpx_call(block, _initDomain, &args, sizeof(args), init);
  }
  hpx_lco_wait(init);
  hpx_lco_delete(init, HPX_NULL);

  unsigned long epoch = 0;

  for (k=0;k<nDoms;k++) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k);
    hpx_call(block, _advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }

  // And wait for each domain to reach the end of its simulation
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains,nDoms\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
}


int main(int argc, char **argv)
{
  hpx_config_t cfg = {
    .cores         = 0,
    .threads       = 0,
    .stack_bytes   = 0,
    .gas           = HPX_GAS_PGAS
  };

  int nDoms, nx, maxcycles,cores;
  // default
  nDoms = 8;
  maxcycles = 10;
  cores = 8;
  cfg.cores = cores;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:D:n:ih")) != -1) {
    switch (opt) {
      case 'c':
        cfg.cores = atoi(optarg);
        cores = cfg.cores;
        break;
      case 't':
        cfg.threads = atoi(optarg);
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
        nDoms = atoi(optarg);
        break;
      case 'i':
        maxcycles = atoi(optarg);
        break;
      case 'h':
        usage(stdout);
        return 0;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  _main      = HPX_REGISTER_ACTION(_main_action);
  _initDomain   = HPX_REGISTER_ACTION(_initDomain_action);
  _advanceDomain   = HPX_REGISTER_ACTION(_advanceDomain_action);

  int input[3];
  input[0] = nDoms;
  input[1] = maxcycles;
  input[2] = cores;
  printf(" Number of domains: %d maxcycles: %d cores: %d\n",nDoms,maxcycles,cores);

  return hpx_run(_main, input, 3*sizeof(int));

  return 0;
}


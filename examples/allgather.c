#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <hpx/hpx.h>

typedef struct {
  int nDoms;
  int maxCycles;
  int cores;
} main_args_t;

typedef struct Domain {
  hpx_addr_t complete;
  hpx_addr_t newdt;
  int nDoms;
  int rank;
  int maxcycles;
  int cycle;
} Domain;

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  int           cores;
  hpx_addr_t complete;
  hpx_addr_t newdt;
} InitArgs;

int allgather_main_action(const main_args_t *args);

void allgather_init_actions(void);
static hpx_action_t _initDomain = 0;
static hpx_action_t _advanceDomain = 0;

static void
_usage(FILE *f, int error) {
  fprintf(f, "Usage: ./example [options] [CYCLES]\n"
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

  // record the newdt allgather
  ld->newdt = args->newdt;

  hpx_gas_unpin(local);

  fflush(stdout);
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
    hpx_gas_unpin(local);
    return HPX_SUCCESS;
  }

  // Compute my gnewdt, and then start the allgather---we can't do this in a
  // non-blocking way at the moment, because it causes problems if we have a
  // proxy set waiting and we issue a get operation.
  //
  // NB: We should implement the setid operation in a way that relaxes this
  //     restriction.
  hpx_addr_t complete = hpx_lco_future_new(0);
  double gnewdt = 3.14*(domain->rank+1) + domain->cycle;
  hpx_lco_allgather_setid(domain->newdt, domain->rank, sizeof(double), &gnewdt,
                          HPX_NULL, complete);
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  // Get the gathered value, and print the debugging string.
  double newdt[domain->nDoms];
  hpx_lco_get(domain->newdt, sizeof(newdt), &newdt);

  ++domain->cycle;
  const unsigned long next = *epoch + 1;
  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
}

int
allgather_main_action(const main_args_t *args)
{
  hpx_time_t t1 = hpx_time_now();

  printf("Number of domains: %d maxCycles: %d cores: %d\n",
         args->nDoms, args->maxCycles, args->cores);
  fflush(stdout);

  hpx_addr_t domain = hpx_gas_global_alloc(args->nDoms, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(args->nDoms);
  hpx_addr_t complete = hpx_lco_and_new(args->nDoms);

  // Call the allgather function here.
  hpx_addr_t newdt = hpx_lco_allgather_new(args->nDoms, sizeof(double));


  for (int i = 0, e = args->nDoms; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .nDoms = args->nDoms,
      .maxcycles = args->maxCycles,
      .cores = args->cores,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _initDomain, &init, sizeof(init), done);
  }

  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  fflush(stdout);

  const unsigned long epoch = 0;
  for (int i = 0, e = args->nDoms; i < e; ++i) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i);
    hpx_call(block, _advanceDomain, &epoch, sizeof(epoch), HPX_NULL);
  }

  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_gas_global_free(domain, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  hpx_shutdown(0);
}

/// Register the actions that we need.
void
allgather_init_actions(void)
{
  _initDomain = HPX_REGISTER_ACTION(_initDomain_action);
  _advanceDomain = HPX_REGISTER_ACTION(_advanceDomain_action);
}

int
main(int argc, char * const argv[argc])
{
  // allocate the default argument structure on the stack
  main_args_t args = {
    .nDoms = 8,
    .maxCycles = 1,
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

  argc -= optind;
  argv += optind;

  switch (argc) {
   case 1:
    args.maxCycles = atoi(argv[0]);
    break;
   case 0:
    break;
   default:
    _usage(stderr, -1);
    return -1;
  }


  // register HPX actions
  allgather_init_actions();

  // register the main action
  hpx_action_t _main = HPX_REGISTER_ACTION(allgather_main_action);

  // run HPX (this copies the args structure)
  return hpx_run(_main, &args, sizeof(args));
}



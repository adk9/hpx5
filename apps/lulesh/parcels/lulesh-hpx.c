#include "lulesh-hpx.h"

static hpx_action_t _main          = 0;
static hpx_action_t _advanceDomain = 0;
static hpx_action_t _initDomain    = 0;
hpx_action_t _SBN1_sends = 0;
hpx_action_t _SBN1_result = 0;
hpx_action_t _SBN3_sends = 0;
hpx_action_t _SBN3_result = 0;

static int _advanceDomain_action(Advance *advance) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  int rank = advance->index;

  SBN1(local,domain,rank);

#if 0
  while ((domain->time < domain->stoptime) && (domain->cycle < domain->maxcycles)) {
    double targetdt = domain->stoptime - domain->time;

    if ((domain->dtfixed <= 0.0) && (domain->cycle != 0)) {
      double gnewdt = 1.0e+20;
      if (domain->dtcourant < gnewdt)
        gnewdt = domain->dtcourant/2.0;
      if (domain->dthydro < gnewdt)
        gnewdt = domain->dthydro*2.0/3.0;

      if (deltaTimeVal[domain->cycle] > gnewdt)
        deltaTimeVal[domain->cycle] = gnewdt;
      deltaTimeCnt[domain->cycle]++;
      //if (deltaTimeCnt[domain->cycle] == domain->nDomains)
        //hpx_lco_future_set(&fut_deltaTime[domain->cycle], 0, (void *)&deltaTimeVal[domain->cycle]);
    }

    CalcForceForNodes(local,domain,rank);
  }
#endif

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _initDomain_action(Advance *advance) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int nx = advance->nx;
  int nDoms = advance->nDoms;
  int maxcycles = advance->maxcycles;
  int cores = advance->cores;
  int index = advance->index;

  int tp = (int) (cbrt(nDoms) + 0.5);

  Init(tp,nx);
  int col = index%tp;
  int row = (index/tp)%tp;
  int plane = index/(tp*tp);
  ld->sem_sbn1 = hpx_lco_sema_new(0);
  ld->sem_sbn3 = hpx_lco_sema_new(0);
  SetDomain(index, col, row, plane, nx, tp, nDoms, maxcycles,ld);
  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

static int _main_action(int *input)
{
  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_time_t t1 = hpx_time_now();

  int nDoms, nx, maxcycles, cores, tp, i, j, k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];
  cores = input[3];

  tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  deltaTimeCnt = malloc(sizeof(int)*maxcycles);
  deltaTimeVal = malloc(sizeof(double)*maxcycles);

  for (i = 0; i < maxcycles; i++) {
    deltaTimeCnt[i] = 0;
    deltaTimeVal[i] = DBL_MAX;
  }


  hpx_addr_t domain = hpx_gas_global_alloc(nDoms,sizeof(Domain));
  hpx_addr_t and = hpx_lco_and_new(nDoms);
  Advance advance[nDoms];
  for (k=0;k<nDoms;k++) {
    advance[k].index = k;
    advance[k].nDoms = nDoms;
    advance[k].nx = nx;
    advance[k].maxcycles = maxcycles;
    advance[k].cores = cores;
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k);
    hpx_call(block, _initDomain, &advance[k], sizeof(advance[k]), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  and = hpx_lco_and_new(nDoms);

  for (k=0;k<nDoms;k++) {
    advance[k].index = k;
    advance[k].nDoms = nDoms;
    advance[k].nx = nx;
    advance[k].maxcycles = maxcycles;
    advance[k].cores = cores;
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k);
    hpx_call(block, _advanceDomain, &advance[k], sizeof(advance[k]), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);

  free(deltaTimeCnt);
  free(deltaTimeVal);

  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains,nDoms\n"
          "\t-x, nx\n"
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
  nx = 15;
  maxcycles = 10;
  cores = 8;
  cfg.cores = cores;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:D:n:x:ih")) != -1) {
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
      case 'x':
        nx = atoi(optarg);
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
  _SBN1_sends = HPX_REGISTER_ACTION(_SBN1_sends_action);
  _SBN1_result = HPX_REGISTER_ACTION(_SBN1_result_action);
  _SBN3_sends = HPX_REGISTER_ACTION(_SBN3_sends_action);
  _SBN3_result = HPX_REGISTER_ACTION(_SBN3_result_action);

  int input[4];
  input[0] = nDoms;
  input[1] = nx;
  input[2] = maxcycles;
  input[3] = cores;
  printf(" Number of domains: %d nx: %d maxcycles: %d cores: %d\n",nDoms,nx,maxcycles,cores);

  return hpx_run(_main, input, 4*sizeof(int));

  return 0;
}


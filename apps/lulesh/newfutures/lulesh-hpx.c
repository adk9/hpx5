#include "lulesh-hpx.h"

static hpx_action_t _main          = 0;
static hpx_action_t _advanceDomain = 0;
static hpx_action_t _initDomain    = 0;

hpx_action_t _SBN1_sends = 0;
hpx_action_t _SBN1_result = 0;
hpx_action_t _SBN3_sends = 0;
hpx_action_t _SBN3_result = 0;
hpx_action_t _PosVel_sends = 0;
hpx_action_t _PosVel_result = 0;
hpx_action_t _MonoQ_sends = 0;
hpx_action_t _MonoQ_result = 0;

static void initdouble(double *input, const size_t size) {  
  assert(sizeof(double) == size);
  *input = 99999999.0;
}

static void mindouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  if ( *output > *input ) *output = *input;
  return;
}

static void initmaxdouble(double *input, const size_t size) {  
  assert(sizeof(double) == size);
  *input = 0;
}

static void maxdouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  if ( *output < *input ) *output = *input;
  return;
}

// perform one epoch of the algorithm
static int _advanceDomain_action(unsigned long *epoch) {
  static hpx_time_t start_time;

  const unsigned long n = *epoch;
  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  // 0. If I've run enough cycles locally, then I want to join the global
  //    complete barrier (stored in my local domain as domain->complete)---this
  //    is the barrier the _main_action() thread is waiting on.
  if ( (domain->time >= domain->stoptime) || (domain->cycle >= domain->maxcycles)) {
    double elapsed_time_local = hpx_time_elapsed_ms(start_time);
    hpx_lco_set(domain->elapsed_ar, sizeof(double), &elapsed_time_local, HPX_NULL, HPX_NULL);

    if ( domain->rank == 0 ) {
      int nx = domain->sizeX;

      double elapsed_time_max;
      hpx_lco_get(domain->elapsed_ar, sizeof(double), &elapsed_time_max);

      printf("\n\nElapsed time = %12.6e\n\n", elapsed_time_max/1000.0);
      printf("Run completed:  \n");
      printf("  Problem size = %d \n"
             "  Iteration count = %d \n"
             "  Final origin energy = %12.6e\n",nx,domain->cycle,domain->e[0]);
      double MaxAbsDiff = 0.0;
      double TotalAbsDiff = 0.0;
      double MaxRelDiff = 0.0;
      int j,k;
      for (j = 0; j < nx; j++) {
        for (k = j + 1; k < nx; k++) {
          double AbsDiff = fabs(domain->e[j*nx + k] - domain->e[k*nx + j]);
          TotalAbsDiff += AbsDiff;

          if (MaxAbsDiff < AbsDiff)
            MaxAbsDiff = AbsDiff;

          double RelDiff = AbsDiff/domain->e[k*nx + j];
          if (MaxRelDiff < RelDiff)
            MaxRelDiff = RelDiff;
        }
      }
      printf("  Testing plane 0 of energy array:\n"
         "  MaxAbsDiff   = %12.6e\n"
         "  TotalAbsDiff = %12.6e\n"
         "  MaxRelDiff   = %12.6e\n\n", MaxAbsDiff, TotalAbsDiff, MaxRelDiff);
    }

    hpx_gas_unpin(local);
    hpx_lco_set(domain->complete, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }
  // on the very first cycle, exchange nodalMass information
  if ( domain->cycle == 0 ) {
    // Send our allreduce messages for epoch n
    SBN1(local, domain, n);

    //  update the domain's epoch, this releases any pending the
    //    _SBN1_result_action messages, which will all acquire and release the
    //    domain lock and then join the sbn1_and reduction for the nth epoch
    hpx_lco_gencount_inc(domain->epoch, HPX_NULL);

    //  wait for the allreduce for this epoch to complete locally
    hpx_lco_wait(domain->sbn1_and);
    //    printf("Done with SBN1 on %d\n", domain->rank);
    hpx_lco_delete(domain->sbn1_and, HPX_NULL);


    start_time = hpx_time_now();
  }


  // 4. Perform the local computation for epoch n
  double targetdt = domain->stoptime - domain->time;
  if ((domain->dtfixed <= 0.0) && (domain->cycle != 0)) {
    double gnewdt = 1.0e+20;
    if (domain->dtcourant < gnewdt)
      gnewdt = domain->dtcourant/2.0;
    if (domain->dthydro < gnewdt)
      gnewdt = domain->dthydro*2.0/3.0;

    // allreduce on gnewdt
    hpx_lco_set(domain->newdt,sizeof(double),&gnewdt,HPX_NULL,HPX_NULL);
  }

  domain->sbn3_and[(n + 1) % 2] = hpx_lco_and_new(domain->recvTF[0]);

  // send messages for epoch n
  CalcForceForNodes(local,domain,domain->rank,n);
  hpx_lco_gencount_inc(domain->epoch, HPX_NULL);
  hpx_lco_wait(domain->sbn3_and[n % 2]);
  //  printf("Done with SBN3 on %d\n", domain->rank);
  hpx_lco_delete(domain->sbn3_and[n % 2], HPX_NULL);

  CalcAccelerationForNodes(domain->xdd, domain->ydd, domain->zdd,
                             domain->fx, domain->fy, domain->fz,
                             domain->nodalMass, domain->numNode);

  ApplyAccelerationBoundaryConditionsForNodes(domain->xdd, domain->ydd, domain->zdd,
                                              domain->symmX, domain->symmY, domain->symmZ,
                                              domain->sizeX);

  if ((domain->dtfixed <= 0.0) && (domain->cycle != 0)) {
    double newdt;
    hpx_lco_get(domain->newdt,sizeof(double),&newdt);
    double olddt = domain->deltatime;
    double ratio = newdt/olddt;
    if (ratio >= 1.0) {
      if (ratio < domain->deltatimemultlb) {
        newdt = olddt;
      } else if (ratio > domain->deltatimemultub) {
        newdt = olddt*domain->deltatimemultub;
      }
    }

    if (newdt > domain->dtmax) {
      newdt = domain->dtmax;
    }

    domain->deltatime = newdt;
  }

  if ((targetdt > domain->deltatime) && (targetdt < 4.0*domain->deltatime/3.0)) {
    targetdt = 2.0*domain->deltatime/3.0;
  }

  if (targetdt < domain->deltatime) {
    domain->deltatime = targetdt;
  }

  CalcVelocityForNodes(domain->xd, domain->yd, domain->zd,
                         domain->xdd, domain->ydd, domain->zdd,
                         domain->deltatime, domain->u_cut, domain->numNode);

  CalcPositionForNodes(domain->x, domain->y, domain->z,
                         domain->xd, domain->yd, domain->zd,
                         domain->deltatime, domain->numNode);

  domain->posvel_and[(n + 1) % 2] = hpx_lco_and_new(domain->recvFF[0]);
  PosVel(local,domain,n);
  hpx_lco_gencount_inc(domain->epoch, HPX_NULL);
  hpx_lco_wait(domain->posvel_and[n % 2]);
  hpx_lco_delete(domain->posvel_and[n % 2], HPX_NULL);

  domain->monoq_and[(n + 1) % 2] = hpx_lco_and_new(domain->recvTT[0]);
  LagrangeElements(local,domain,n);

  CalcTimeConstraintsForElems(domain);

  domain->time += domain->deltatime;

  domain->cycle++;

  // don't need this domain to be pinned anymore---let it move
  hpx_gas_unpin(local);

  //  printf("============================================================== domain %d iter %d\n", domain->rank, n);

  // 5. spawn the next epoch
  unsigned long next = n + 1;

  return hpx_call(local, _advanceDomain, &next, sizeof(next), HPX_NULL);
}

static int _initDomain_action(InitArgs *init) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  int nx        = init->nx;
  int nDoms     = init->nDoms;
  int maxcycles = init->maxcycles;
  int index     = init->index;
  int tp        = (int) (cbrt(nDoms) + 0.5);

  Init(tp,nx);
  int col      = index%tp;
  int row      = (index/tp)%tp;
  int plane    = index/(tp*tp);
  ld->base = init->base;
  ld->sem_sbn1 = hpx_lco_sema_new(1);
  ld->sem_sbn3 = hpx_lco_sema_new(1);
  ld->sem_posvel = hpx_lco_sema_new(1);
  ld->sem_monoq = hpx_lco_sema_new(1);
  SetDomain(index, col, row, plane, nx, tp, nDoms, maxcycles,ld);

  ld->newdt = init->newdt;
  ld->elapsed_ar = init->elapsed_ar;

  // remember the LCO we're supposed to set when we've completed maxcycles
  ld->complete = init->complete;

  // allocate the domain's generation counter
  //
  // NB: right now, we're only doing an allreduce, so there is only ever one
  //     generation waiting---if we end up using this counter inside of the
  //     allreduce boundary (i.e., timestamp), then we need to allocate the
  //     right number of inplace generations in this constructor
  ld->epoch = hpx_lco_gencount_new(0);

  // allocate the initial allreduce and gate
  ld->sbn1_and = hpx_lco_and_new(ld->recvTF[0]);

  ld->sbn3_and[0] = hpx_lco_and_new(ld->recvTF[0]);
  ld->sbn3_and[1] = HPX_NULL;

  ld->posvel_and[0] = hpx_lco_and_new(ld->recvFF[0]);
  ld->posvel_and[1] = HPX_NULL;

  ld->monoq_and[0] = hpx_lco_and_new(ld->recvTT[0]);
  ld->monoq_and[1] = HPX_NULL;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(int *input)
{
  hpx_time_t tick = hpx_time_now();
  //printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_time_t t1 = hpx_time_now();

  int nDoms, nx, maxcycles, tp, i, j, k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];

  tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  hpx_addr_t domain = hpx_gas_global_alloc(nDoms,sizeof(Domain));
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  // Initialize the domains
  hpx_addr_t init = hpx_lco_and_new(nDoms);
  hpx_addr_t newdt = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(double),
					   (hpx_commutative_associative_op_t)mindouble,
					   (void (*)(void *, const size_t size)) initdouble);
  hpx_addr_t elapsed_ar;
  elapsed_ar = hpx_lco_allreduce_new(nDoms, 1, sizeof(double),
				     (hpx_commutative_associative_op_t)maxdouble,
				     (void (*)(void *, const size_t size)) initmaxdouble);
  for (k=0;k<nDoms;k++) {
    InitArgs args = {
      .base = domain,
      .elapsed_ar = elapsed_ar,
      .index = k,
      .nDoms = nDoms,
      .nx = nx,
      .maxcycles = maxcycles,
      .complete = complete,
      .newdt = newdt
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k, sizeof(Domain));
    hpx_call(block, _initDomain, &args, sizeof(args), init);
  }
  hpx_lco_wait(init);
  hpx_lco_delete(init, HPX_NULL);

  // Spawn the first epoch, _advanceDomain will recursively spawn each epoch.
  unsigned long epoch = 0;

  for (k=0;k<nDoms;k++) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k, sizeof(Domain));
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
          "\t-x, nx\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
}


int main(int argc, char **argv)
{
  int nDoms, nx, maxcycles;
  // default
  nDoms = 8;
  nx = 15;
  maxcycles = 10;

 int e = hpx_init(&argc, &argv); 
 if (e) {
   fprintf(stderr, "Failed to initialize hpx\n");
   return -1;
 }
 
  int opt = 0;
  while ((opt = getopt(argc, argv, "n:x:i:h?")) != -1) {
    switch (opt) {
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

  _main      = HPX_REGISTER_ACTION(_main_action);
  _initDomain   = HPX_REGISTER_ACTION(_initDomain_action);
  _advanceDomain   = HPX_REGISTER_ACTION(_advanceDomain_action);
  _SBN1_sends = HPX_REGISTER_ACTION(_SBN1_sends_action);
  _SBN1_result = HPX_REGISTER_ACTION(_SBN1_result_action);
  _SBN3_sends = HPX_REGISTER_ACTION(_SBN3_sends_action);
  _SBN3_result = HPX_REGISTER_ACTION(_SBN3_result_action);
  _PosVel_sends = HPX_REGISTER_ACTION(_PosVel_sends_action);
  _PosVel_result = HPX_REGISTER_ACTION(_PosVel_result_action);
  _MonoQ_sends = HPX_REGISTER_ACTION(_MonoQ_sends_action);
  _MonoQ_result = HPX_REGISTER_ACTION(_MonoQ_result_action);

  int input[3];
  input[0] = nDoms;
  input[1] = nx;
  input[2] = maxcycles;
  printf(" Number of domains: %d nx: %d maxcycles: %d\n",nDoms,nx,maxcycles);

  return hpx_run(_main, input, 3*sizeof(int));

  return 0;
}


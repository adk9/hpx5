#include "lulesh-hpx.h"

int main(int argc, char **argv)
{
  int nDoms, nx, maxcycles, cores, tp, i, j, k; 
  hpx_config_t cfg; 
  hpx_future_t **futs; 

  if (argc == 1) {
    // use default values 
    nDoms = 8;
    nx = 15;
    maxcycles = 10;
    cores = 8;
  } else if (argc == 5) {
    nDoms = atoi(argv[1]);
    nx = atoi(argv[2]);
    maxcycles = atoi(argv[3]);
    cores = atoi(argv[4]);
  } else {
    fprintf(stderr, "Run make help for usage.\n"); 
    return -1;
  }

  tp = (int) (cbrt(nDoms) + 0.5); 
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    return -1;
  }

  Init(tp, nx);
  DOMAINS = hpx_alloc(sizeof(Domain)*nDoms); 
  for (i = 0; i < nDoms; i++) {
    int col = i%tp; 
    int row = (i/tp)%tp; 
    int plane = i/(tp*tp);
    SetDomain(i, col, row, plane, nx, tp, nDoms, maxcycles); 
  }

  // setup hpx runtime
  hpx_init(); 
  hpx_config_init(&cfg);
  hpx_config_set_thread_suspend_policy(&cfg, HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL);
  hpx_config_set_cores(&cfg, cores);
  ctx = hpx_ctx_create(&cfg);
  futs = hpx_alloc(sizeof(hpx_future_t *)*nDoms); 

  fut_deltaTime = hpx_alloc(sizeof(hpx_future_t)*maxcycles);
  deltaTimeCnt = hpx_alloc(sizeof(int)*maxcycles);
  deltaTimeVal = hpx_alloc(sizeof(double)*maxcycles); 

  for (i = 0; i < maxcycles; i++) {
    hpx_lco_future_init(&fut_deltaTime[i]);
    deltaTimeCnt[i] = 0;
    deltaTimeVal[i] = DBL_MAX; 
  }

  hpx_lco_mutex_init(&mtx, 0);

  // exchange nodalMass information
  for (i = 0; i < nDoms; i++) 
    futs[i] = hpx_thread_create(ctx, 0, SBN1, (void *)i, NULL);

  for (i = 0; i < nDoms; i++) {
    hpx_thread_wait(futs[i]); 
    hpx_lco_future_destroy(futs[i]);
  }

  // create a thread for each domain and do time marching
  for (i = 0; i < nDoms; i++) 
    futs[i] = hpx_thread_create(ctx, 0, AdvanceDomain, (void *)i, NULL);

  for (i = 0; i < nDoms; i++)
    hpx_thread_wait(futs[i]);

  Domain *domain = &DOMAINS[0]; 
  printf("Run completed:\n" 
	 "  Problem size = %d \n"
	 "  Iteration count = %d \n"
	 "  Final origin energy = %12.6e\n", nx, domain->cycle, domain->e[0]);

  double MaxAbsDiff = 0.0;
  double TotalAbsDiff = 0.0;
  double MaxRelDiff = 0.0;
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

  // cleanup
  for (i = 0; i < maxcycles; i++)
    hpx_lco_future_destroy(&fut_deltaTime[i]);
  hpx_free(fut_deltaTime);
  hpx_free(deltaTimeCnt);
  hpx_free(deltaTimeVal);

  for (i = 0; i < nDoms; i++) 
    DestroyDomain(&DOMAINS[i]);
  hpx_free(DOMAINS);

  for (i = 0; i < nDoms; i++) 
    hpx_lco_future_destroy(futs[i]);
  hpx_free(futs);

  hpx_ctx_destroy(ctx);
  return 0;
}

void AdvanceDomain(void *param)
{
  long rank = (long) param; 
  Domain *domain = &DOMAINS[rank]; 
  int maxcycles = domain->maxcycles; 

  while((domain->time < domain->stoptime) && (domain->cycle < maxcycles)) {
    double targetdt = domain->stoptime - domain->time; 

    if ((domain->dtfixed <= 0.0) && (domain->cycle != 0)) {
      double gnewdt = 1.0e+20;
      if (domain->dtcourant < gnewdt) 
	gnewdt = domain->dtcourant/2.0;
      if (domain->dthydro < gnewdt)
	gnewdt = domain->dthydro*2.0/3.0;
      
      hpx_lco_mutex_lock(&mtx);
      if (deltaTimeVal[domain->cycle] > gnewdt)
	deltaTimeVal[domain->cycle] = gnewdt; 
      deltaTimeCnt[domain->cycle]++;
      if (deltaTimeCnt[domain->cycle] == domain->nDomains) 
	hpx_lco_future_set(&fut_deltaTime[domain->cycle], 0, (void *)&deltaTimeVal[domain->cycle]);
      hpx_lco_mutex_unlock(&mtx);
    }

    CalcForceForNodes(rank);

    CalcAccelerationForNodes(domain->xdd, domain->ydd, domain->zdd, 
			     domain->fx, domain->fy, domain->fz, 
			     domain->nodalMass, domain->numNode); 

    ApplyAccelerationBoundaryConditionsForNodes(domain->xdd, domain->ydd, domain->zdd, 
						domain->symmX, domain->symmY, domain->symmZ, 
						domain->sizeX);

    if ((domain->dtfixed <= 0.0) && (domain->cycle != 0)) {
      hpx_thread_wait(&fut_deltaTime[domain->cycle]); 
      double *newdt = (double *) hpx_lco_future_get_value(&fut_deltaTime[domain->cycle]);
    
      double olddt = domain->deltatime; 
      double ratio = *newdt/olddt; 
      if (ratio >= 1.0) {
	if (ratio < domain->deltatimemultlb) {
	  *newdt = olddt; 
	} else if (ratio > domain->deltatimemultub) {
	  *newdt = olddt*domain->deltatimemultub;
	}
      }
      
      if (*newdt > domain->dtmax) {
	*newdt = domain->dtmax;
      }
      
      domain->deltatime = *newdt;
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
    PosVel(rank);

    LagrangeElements(rank); 
    
    CalcTimeConstraintsForElems(domain);

    domain->time += domain->deltatime; 

    domain->cycle++; 
  }

  hpx_thread_exit(NULL);
}


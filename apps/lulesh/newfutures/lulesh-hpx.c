// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

#include "lulesh-hpx.h"

/* command line options */
static bool         _text = false;            //!< send text data with the ping
static bool      _verbose = false;            //!< print to the terminal

/* actions */
static hpx_action_t _main = 0;
static hpx_action_t _evolve = 0;


/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: lulesh [options]\n"
          "\t-c, the number of cores to run on\n"
          "\t-t, the number of scheduler threads\n"
          "\t-m, send text in message\n"
          "\t-v, print verbose output \n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains,nDoms\n"
          "\t-x, nx\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
}

static void _register_actions(void);

/* utility macros */
#define CHECK_NOT_NULL(p, err)                                \
  do {                                                        \
    if (!p) {                                                 \
      fprintf(stderr, err);                                   \
      hpx_shutdown(1);                                        \
    }                                                         \
  } while (0)

#define RANK_PRINTF(format, ...)                                        \
  do {                                                                  \
    if (_verbose)                                                       \
      printf("\t%d,%d: " format, hpx_get_my_rank(), hpx_get_my_thread_id(), \
             __VA_ARGS__);                                              \
  } while (0)

/// Initialize a double zero.
static void
_initDouble(double *input)
{
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void
_maxDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

int main(int argc, char *argv[]) {
  hpx_config_t cfg = HPX_CONFIG_DEFAULTS;
  //cfg.heap_bytes = 2e9;

  int nDoms, nx, maxcycles,cores;
  // default
  nDoms = 8;
  nx = 15;
  maxcycles = 10;
  cores = 10;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dmvh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      cores = cfg.cores;
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'm':
      _text = true;
     case 'v':
      _verbose = true;
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
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  int e = hpx_init(&cfg);
  if (e) {
    fprintf(stderr, "Failed to initialize hpx\n");
    return -1;
  }

  _register_actions();

  //const char *network = hpx_get_network_id();

  int input[4];
  input[0] = nDoms;
  input[1] = nx;
  input[2] = maxcycles;
  input[3] = cores;
  printf(" Number of domains: %d nx: %d maxcycles: %d cores: %d\n",nDoms,nx,maxcycles,cores);

  hpx_time_t start = hpx_time_now();
  e = hpx_run(_main, input, 4*sizeof(int));
  double elapsed = (double)hpx_time_elapsed_ms(start);
  printf("average elapsed:   %f ms\n", elapsed);
  return e;
}

static int _action_main(int *input) {
  printf("In main\n");

  int nDoms, nx, maxcycles, cores,k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];
  cores = input[3];

  int tp = (int) (cbrt(nDoms) + 0.5);
  if (tp*tp*tp != nDoms) {
    fprintf(stderr, "Number of domains must be a cube of an integer (1, 8, 27, ...)\n");
    hpx_shutdown(HPX_ERROR);
  }

  hpx_netfutures_init();
  hpx_netfuture_t sbn1 = hpx_lco_netfuture_new_all(26*nDoms,(nx+1)*(nx+1)*(nx+1)*sizeof(double));
  hpx_netfuture_t sbn3 = hpx_lco_netfuture_new_all(2*26*nDoms,(nx+1)*(nx+1)*(nx+1)*sizeof(double));
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  hpx_addr_t newdt = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(double),
                                           (hpx_commutative_associative_op_t)_maxDouble,
                                           (void (*)(void *, const size_t size)) _initDouble);

  for (k=0;k<nDoms;k++) {
    InitArgs args = {
      .index = k,
      .nDoms = nDoms,
      .nx = nx,
      .maxcycles = maxcycles,
      .cores = cores,
      .newdt = newdt,
      .sbn1 = sbn1,
      .sbn3 = sbn3
    };
    hpx_call(HPX_THERE(k), _evolve, &args, sizeof(args), complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  printf("finished main\n");
  hpx_shutdown(HPX_ERROR);
  return HPX_SUCCESS;
}

static int _action_evolve(InitArgs *init) {

  Domain *ld;
  ld = (Domain *) malloc(sizeof(Domain)); 

  int nx        = init->nx;
  int nDoms     = init->nDoms;
  int maxcycles = init->maxcycles;
  //int cores     = init->cores;
  int index     = init->index;
  int tp        = (int) (cbrt(nDoms) + 0.5);

  Init(tp,nx);
  int col      = index%tp;
  int row      = (index/tp)%tp;
  int plane    = index/(tp*tp);
  hpx_netfuture_t sbn1 = init->sbn1;
  hpx_netfuture_t sbn3 = init->sbn3;
  hpx_addr_t lco_newdt = init->newdt;
  
  SetDomain(index, col, row, plane, nx, tp, nDoms, maxcycles,ld);

  while ((ld->time < ld->stoptime) && (ld->cycle < ld->maxcycles)) {
    // on the very first cycle, exchange nodalMass information
    if ( ld->cycle == 0 ) {
      SBN1(ld,sbn1);
    }
    double targetdt = ld->stoptime - ld->time;
    if ((ld->dtfixed <= 0.0) && (ld->cycle != 0)) {
      double gnewdt = 1.0e+20;
      if (ld->dtcourant < gnewdt)
        gnewdt = ld->dtcourant/2.0;
      if (ld->dthydro < gnewdt)
        gnewdt = ld->dthydro*2.0/3.0;

      // allreduce on gnewdt
      hpx_lco_set(lco_newdt, sizeof(double), &gnewdt, HPX_NULL, HPX_NULL);
    }

  //  CalcForceForNodes(sbn3,ld,ld->rank);
    if ((ld->dtfixed <= 0.0) && (ld->cycle != 0)) {
      double newdt;
      hpx_lco_get(lco_newdt,sizeof(double),&newdt);
      double olddt = ld->deltatime;
      double ratio = newdt/olddt;
      if (ratio >= 1.0) {
        if (ratio < ld->deltatimemultlb) {
          newdt = olddt;
        } else if (ratio > ld->deltatimemultub) {
          newdt = olddt*ld->deltatimemultub;
        }
      }

      if (newdt > ld->dtmax) {
        newdt = ld->dtmax;
      }

      ld->deltatime = newdt;
    }

    ld->time += ld->deltatime;

    ld->cycle++;
  }

  if ( ld->rank == 0 ) {
    int nx = ld->sizeX;
    printf("  Problem size = %d \n"
           "  Iteration count = %d \n"
           "  Final origin energy = %12.6e\n",nx,ld->cycle,ld->e[0]);
    double MaxAbsDiff = 0.0;
    double TotalAbsDiff = 0.0;
    double MaxRelDiff = 0.0;
    int j,k;
    for (j = 0; j < nx; j++) {
      for (k = j + 1; k < nx; k++) {
        double AbsDiff = fabs(ld->e[j*nx + k] - ld->e[k*nx + j]);
        TotalAbsDiff += AbsDiff;

        if (MaxAbsDiff < AbsDiff)
          MaxAbsDiff = AbsDiff;

        double RelDiff = AbsDiff/ld->e[k*nx + j];
        if (MaxRelDiff < RelDiff)
          MaxRelDiff = RelDiff;
      }
    }
    printf("  Testing plane 0 of energy array:\n"
       "  MaxAbsDiff   = %12.6e\n"
       "  TotalAbsDiff = %12.6e\n"
       "  MaxRelDiff   = %12.6e\n\n", MaxAbsDiff, TotalAbsDiff, MaxRelDiff);
  }

  free(ld);
  return HPX_SUCCESS;
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  _main = HPX_REGISTER_ACTION(_action_main);
  _evolve = HPX_REGISTER_ACTION(_action_evolve);
//  _ping = HPX_REGISTER_ACTION(_action_ping);
//  _pong = HPX_REGISTER_ACTION(_action_pong);
}

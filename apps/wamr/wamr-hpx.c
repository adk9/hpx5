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

#include "wamr-hpx.h"

/* command line options */
static bool         _text = false;            //!< send text data with the ping
static bool      _verbose = false;            //!< print to the terminal

/* actions */
static hpx_action_t _main = 0;
static hpx_action_t _evolve = 0;


/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: lulesh [options]\n"
          "\t-n, number of domains,nDoms\n"
          "\t-x, nx\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(stream);
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
  int nDoms, nx, maxcycles;
  // default
  nDoms = 8;
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
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  _register_actions();

  int input[3];
  input[0] = nDoms;
  input[1] = maxcycles;
  printf(" Number of domains: %d maxcycles: %d\n",nDoms,maxcycle);

  hpx_time_t start = hpx_time_now();
  e = hpx_run(_main, input, 3*sizeof(int));
  double elapsed = (double)hpx_time_elapsed_ms(start);
  printf("average elapsed:   %f ms\n", elapsed);
  return e;
}

static int _action_main(int *input) {
  printf("In main\n");

  int nDoms, nx, maxcycles,k;
  nDoms = input[0];
  maxcycles = input[1];

  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  for (k=0;k<nDoms;k++) {
    InitArgs args = {
      .index = k,
      .nDoms = nDoms,
      .maxcycles = maxcycles,
    };
    hpx_call(HPX_THERE(k % hpx_get_num_ranks()), _evolve, &args, sizeof(args), complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  printf("finished main\n");
  hpx_shutdown(HPX_ERROR);
  return HPX_SUCCESS;
}

static int _action_evolve(InitArgs *init) {
  int nDoms     = init->nDoms;
  int maxcycles = init->maxcycles;
  int index     = init->index;

  Domain *ld;
  ld = (Domain *) malloc(sizeof(Domain));

  ld->nDoms = nDoms;
  ld->myindex = index;

  problem_init(ld);

  // generate initial adaptive grid
  storage_init(ld);
  create_full_grids(ld);
  create_adap_grids(ld);
  printf("max_level = %d\n", ld->max_level);

  // configure derivative stencil for all active points
  deriv_stencil_config(ld->t0,ld);

  double t = ld->t0;
  int count = 0;

  while (count < 1 && t < ld->tf) {
    double dt = get_global_dt(ld);
    dt = fmin(dt, ld->tf - t);

    apply_time_integrator(t, dt,ld);
    
    // update simulation time stamp. within the time integrator, time step is
    // updated incremently throughout all the generations. the following for
    // loop forces the time stamp computed here is exactly the same as computed
    // inside the time integrator 
    for (int igen = 0; igen < n_gen; igen++)
      t += dt / n_gen;

    count++;
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

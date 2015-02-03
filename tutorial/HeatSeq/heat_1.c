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

// The program heats one of the sides as the initial condition, and iterates to
// study how the heat transfers across the surface.
#include <limits.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "hpx/hpx.h"

#define N 30

hpx_addr_t grid;
hpx_addr_t new_grid;

typedef struct {
  hpx_addr_t grid;
  hpx_addr_t new_grid;
}global_args_t;

typedef struct {
  int rank;
  hpx_addr_t runtimes;
  hpx_addr_t dTmax;
}Domain;

typedef struct {
  int index;
  hpx_addr_t runtimes;
  hpx_addr_t dTmax;
}InitArgs;

#define BLOCKSIZE sizeof(double)

static hpx_action_t _main          = 0;
static hpx_action_t _initGlobals   = 0;
static hpx_action_t _initDomain    = 0;
static hpx_action_t _initGrid      = 0;
static hpx_action_t _updateGrid    = 0;
static hpx_action_t _write_double  = 0;
static hpx_action_t _read_double   = 0;
static hpx_action_t _stencil       = 0;
static hpx_action_t _spawn_stencil = 0;

static void _usage(FILE *f, int error) {
  fprintf(f, "Usage: Heat Sequence [options]\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
  exit(error);
}
static void _register_actions(void);

/// Initialize a double zero
static void initDouble(double *input) {
  *input = 0;
}

/// Update *lhs with with the max(lhs, rhs);
static void maxDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs > *rhs) ? *lhs : *rhs;
}

static int _write_double_action(double *d) {
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_try_pin(target, (void**)&addr)) 
    return HPX_RESEND;
  
  *addr = d[0];
  hpx_gas_unpin(target);
  hpx_thread_continue(sizeof(double), &d[1]);
}

static int _read_double_action(void *unused) {
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_try_pin(target, (void**)&addr))
    return HPX_RESEND;
  
  double d = *addr;

  hpx_gas_unpin(target);
  HPX_THREAD_CONTINUE(d);
}

static int offset_of(int i, int j) {
  return (i * (N + 2) * sizeof(double)) + (j * sizeof(double));
}

static int _stencil_action(int *ij) {
  // read the value in this cell
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_try_pin(target, (void**)&addr)) 
    return HPX_RESEND;
  
  double v = *addr;
  hpx_gas_unpin(target);

  // read the four neighbor cells asynchronously
  double vals[4] = {
    0.0,
    0.0,
    0.0,
    0.0
  };

  void *addrs[4] = {
    &vals[0],
    &vals[1],
    &vals[2],
    &vals[3]
  };

  hpx_addr_t futures[4] = {
    hpx_lco_future_new(sizeof(double)),
    hpx_lco_future_new(sizeof(double)),
    hpx_lco_future_new(sizeof(double)),
    hpx_lco_future_new(sizeof(double))
  };

  int sizes[4] = {
    sizeof(double),
    sizeof(double),
    sizeof(double),
    sizeof(double)
  };

  int i = ij[0];
  int j = ij[1];

  hpx_addr_t neighbors[4] = {
    hpx_addr_add(grid, offset_of(i + 1, j), BLOCKSIZE),
    hpx_addr_add(grid, offset_of(i - 1, j), BLOCKSIZE),
    hpx_addr_add(grid, offset_of(i, j - 1), BLOCKSIZE),
    hpx_addr_add(grid, offset_of(i, j + 1), BLOCKSIZE)
  };

  for (int i = 0; i < 4; ++i) {
    hpx_call(neighbors[i], _read_double, futures[i], &vals[i], sizes[i]);
  }

  hpx_lco_get_all(4, futures, sizes, addrs, NULL);

  for (int n = 0; n < 4; ++n) {
    hpx_lco_delete(futures[n], HPX_NULL);
  }

  // compute the new T and dT
  double T = 0.25 * (vals[0] + vals[1] + vals[2] + vals[3]); // stencil
  double dT = T - v; // local variation

  // write out the new T and continue the dT for the min reduction
  double cont_args[2] = { T, fabs(dT) };
  hpx_addr_t new_grid_addr = hpx_addr_add(new_grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(new_grid_addr, _write_double, NULL, NULL, cont_args,
              sizeof(cont_args));
  return HPX_SUCCESS;
}

static void spawn_stencil_args_init(void *out, const int i, const void *env) {
  int *ij = out;

  ij[0] = 1+((i-1)/(N));
  ij[1] = 1+((i-1)%(N));
}

static int _spawn_stencil_action(int *ij) {
  int i = ij[0];
  int j = ij[1];

  hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(cell, _stencil, NULL, NULL, ij, 2 * sizeof(int));
  return HPX_SUCCESS;
}

static int _updateGrid_action(void *args) {
  struct timeval ts_st, ts_end;
  double time, max_time;
  double dTmax, epsilon, dTmax_global;
  int finished;
  int nr_iter;

  hpx_addr_t local = hpx_thread_current_target();
  Domain *domain = NULL;
  if (!hpx_gas_try_pin(local, (void**)&domain))
    return HPX_RESEND;

  /* Set the precision wanted */
  epsilon  = 0.001;
  finished = 0;
  nr_iter = 0;

  gettimeofday( &ts_st, NULL );

  do {
    dTmax = 0.0;

    hpx_addr_t max = hpx_lco_allreduce_new(N * N, 1, sizeof(dTmax), 
                         (hpx_monoid_op_t)maxDouble, 
                         (hpx_monoid_id_t)initDouble);
    //for (int i = 1; i < N + 1; i++) {
    //  for (int j = 1; j < N + 1; j++) {
    //    int args[2] = { i, j };
    //     hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
    //     hpx_call(cell, _stencil, min, args, sizeof(args));
    //   }
    // }
    hpx_par_call(_spawn_stencil, 1, (N)*(N)+1 , N*N, (N+2)*(N+2), 
                 sizeof(int), spawn_stencil_args_init, 0, NULL, max);
   
    hpx_lco_get(max, sizeof(dTmax), &dTmax);

    //printf("%g\n", dTmax);

    // reduce to get the max of dTmax 
    hpx_lco_set(domain->dTmax, sizeof(double), &dTmax, HPX_NULL, HPX_NULL);
    hpx_lco_get(domain->dTmax, sizeof(double), &dTmax_global);
   
    dTmax = dTmax_global;

    if (dTmax < epsilon ) // is the precision reached good enough ?
      finished = 1;
    else {
      hpx_addr_t tmp_grid = grid;
      grid = new_grid;
      new_grid = tmp_grid;
    }
    nr_iter++;
  } while (finished == 0);

  gettimeofday(&ts_end, NULL);

  /* compute the execution time */
  time = ts_end.tv_sec + (ts_end.tv_usec / 1000000.0);
  time -= ts_st.tv_sec + (ts_st.tv_usec / 1000000.0);

  printf("Rank = #%d: %d iteration in %.3lf sec\n", domain->rank, nr_iter, 
          time); 

  hpx_lco_set(domain->runtimes, sizeof(double), &time, HPX_NULL, HPX_NULL);
  hpx_lco_get(domain->runtimes, sizeof(double), &max_time);

  if (domain->rank == 0) {
    printf("Max Execution time =  %.3lf sec\n", max_time); 
  }
  return HPX_SUCCESS;
}

static int _initGlobals_action(global_args_t *args) {
  grid = args->grid;
  new_grid = args->new_grid;
  return HPX_SUCCESS;
}

void init_globals(hpx_addr_t grid, hpx_addr_t new_grid) {
  hpx_addr_t init_lco = hpx_lco_future_new(0);
  const global_args_t init_args = { .grid = grid, .new_grid = new_grid };
  hpx_bcast(_initGlobals, init_lco, &init_args, sizeof(init_args));
  hpx_lco_wait(init_lco);
  hpx_lco_delete(init_lco, HPX_NULL);
}

static int _initDomain_action(const InitArgs *args)
{
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->rank = args->index;
  ld->runtimes = args->runtimes;
  ld->dTmax = args->dTmax;

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

static int _initGrid_action(void *args) {
  hpx_addr_t local = hpx_thread_current_target();
  double *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  /* Heat one side of the solid */
  for (int j = 1; j < N + 1; j++) {
    ld[j] = 1.0;
  }

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(int *input)
{
  grid = hpx_gas_global_calloc(HPX_LOCALITIES, (N+2)*(N+2)*sizeof(double));
  new_grid = hpx_gas_global_calloc(HPX_LOCALITIES, (N+2)*(N+2)*sizeof(double));

  hpx_addr_t domain = hpx_gas_global_alloc(HPX_LOCALITIES, sizeof(Domain));
  hpx_addr_t done = hpx_lco_and_new(HPX_LOCALITIES);  
  hpx_addr_t complete = hpx_lco_and_new(HPX_LOCALITIES);

  hpx_addr_t gDone   = hpx_lco_future_new(0);
  hpx_call(grid, _initGrid, gDone, NULL, 0);
  hpx_lco_wait(gDone);
  hpx_lco_delete(gDone, HPX_NULL);

  hpx_addr_t nDone   = hpx_lco_future_new(0);
  hpx_call(new_grid, _initGrid, nDone, NULL, 0);
  hpx_lco_wait(nDone);
  hpx_lco_delete(nDone, HPX_NULL);

  init_globals(grid, new_grid);

  hpx_addr_t runtimes = hpx_lco_allreduce_new(HPX_LOCALITIES, HPX_LOCALITIES, 
                           sizeof(double),
                          (hpx_monoid_op_t)maxDouble,
                          (hpx_monoid_id_t)initDouble);

  hpx_addr_t dTmax = hpx_lco_allreduce_new(HPX_LOCALITIES, HPX_LOCALITIES, 
                           sizeof(double),
                          (hpx_monoid_op_t)maxDouble,
                          (hpx_monoid_id_t)initDouble); 

  for (int i = 0, e = HPX_LOCALITIES; i < e; ++i) {
    InitArgs init = {
      .index = i,
      .runtimes = runtimes,
      .dTmax = dTmax
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * i, 
                                    sizeof(Domain));
    hpx_call(block, _initDomain, done, &init, sizeof(init));
  }
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);

  for (int i = 0; i < HPX_LOCALITIES; i++) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain)*i, sizeof(Domain)); 
    hpx_call(block, _updateGrid, complete, NULL, 0);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL); 

  hpx_gas_free(grid, HPX_NULL);
  hpx_gas_free(new_grid, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  HPX_REGISTER_ACTION(_main_action, &_main);
  HPX_REGISTER_ACTION(_initGlobals_action, &_initGlobals);
  HPX_REGISTER_ACTION(_initDomain_action, &_initDomain);
  HPX_REGISTER_ACTION(_initGrid_action, &_initGrid);
  HPX_REGISTER_ACTION(_updateGrid_action, &_updateGrid);
  HPX_REGISTER_ACTION(_write_double_action, &_write_double);
  HPX_REGISTER_ACTION(_read_double_action, &_read_double);
  HPX_REGISTER_ACTION(_stencil_action, &_stencil);
  HPX_REGISTER_ACTION(_spawn_stencil_action, &_spawn_stencil);
}

// Main routine
int main(int argc, char *argv[])
{
  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  // parse the command line
  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
       _usage(stdout, EXIT_SUCCESS);
     case '?':
     default:
       _usage(stderr, EXIT_FAILURE);
    }
  }

  _register_actions();

  return hpx_run(&_main, NULL, 0);
}

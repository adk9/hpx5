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

#define BLOCKSIZE sizeof(double)

static hpx_action_t _main          = 0;
static hpx_action_t _initialize    = 0;
static hpx_action_t _write_double  = 0;
static hpx_action_t _read_double   = 0;
static hpx_action_t _stencil       = 0;
static hpx_action_t _spawn_stencil = 0;
static hpx_action_t _row_stencil   = 0;

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

/// Update *lhs with with the min(lhs, rhs);
static void minDouble(double *lhs, const double *rhs) {
  *lhs = (*lhs < *rhs) ? *lhs : *rhs;
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
  hpx_thread_continue(sizeof(d), &d);
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
    hpx_call(neighbors[i], _read_double, &vals[i], sizes[i], futures[i]);
  }

  hpx_lco_get_all(4, futures, sizes, addrs, NULL);

  for (int n = 0; n < 4; ++i) {
    hpx_lco_delete(futures[n], HPX_NULL);
  }

  // compute the new T and dT
  double T = 0.25 * (vals[0] + vals[1] + vals[2] + vals[3]); // stencil
  double dT = T - v; // local variation

  // write out the new T and continue the dT for the min reduction
  double cont_args[2] = { T, fabs(dT) };
  hpx_addr_t new_grid_addr = hpx_addr_add(new_grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(new_grid_addr, _write_double, cont_args, sizeof(cont_args), NULL,
              NULL);
  return HPX_SUCCESS;
}

static void spawn_stencil_args_init(void *out, const int j, const void *env) {
  int *ij = out;
  const int *i = env;
  ij[0] = *i;
  ij[1] = j;
}

static int _spawn_stencil_action(int *ij) {
  int i = ij[0];
  int j = ij[1];

  hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(cell, _stencil, ij, 2*sizeof(int), NULL, NULL);
  return HPX_SUCCESS;
}

static void row_stencil_args_init(void *out, const int i, const void *env) {
  *((int*)out) = i;
}

static int _row_stencil_action(int *i) {
  hpx_par_call_sync(_spawn_stencil, 1, N + 1, N, N + 2, 2 * sizeof(int),
                    spawn_stencil_args_init, sizeof(int), i);
  return HPX_SUCCESS;
}

static int update_grid() {
  struct timeval ts_st, ts_end;
  double time;
  double dTmax, epsilon;
  int finished;
  int nr_iter;

  /* Set the precision wanted */
  epsilon  = 0.0001;
  finished = 0;
  nr_iter = 0;

  gettimeofday( &ts_st, NULL );

  do {
    dTmax = 0.0;

    hpx_addr_t min = hpx_lco_allreduce_new(N * N, 1, sizeof(dTmax), 
                         (hpx_commutative_associative_op_t)minDouble, 
                         (void (*)(void *, const size_t size))initDouble);
     //for (int i = 1; i < N + 1; i++) {
     //  for (int j = 1; j < N + 1; j++) {
     //    int args[2] = { i, j };
     //    hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
     //    hpx_call(cell, _stencil, args, sizeof(args), min);
     //  }
     //}
    hpx_par_call(_row_stencil, 1, N + 1 , N, N + 2, sizeof(int),
                 row_stencil_args_init, 0, NULL, min);
    hpx_lco_get(min, sizeof(dTmax), &dTmax);

    if (dTmax < epsilon ) // is the precision reached good enough ?
      finished = 1;
    else {
      hpx_addr_t done = hpx_lco_future_new(0);
      hpx_gas_memcpy(grid, new_grid, ((N+2)*(N+2)*sizeof(double)), done);
      hpx_lco_wait(done);
      hpx_lco_delete(done, HPX_NULL);
    }
    nr_iter++;
  } while (finished == 0);

  gettimeofday(&ts_end, NULL);

  /* compute the execution time */
  time = ts_end.tv_sec + (ts_end.tv_usec / 1000000.0);
  time -= ts_st.tv_sec + (ts_st.tv_usec / 1000000.0);

  if (HPX_LOCALITY_ID == 0) {
    printf("%d iterations in %.3lf sec\n", nr_iter, time ); /* and prints it */
  }
  printf("Done!\n");
  return HPX_SUCCESS;
}

static int _initialize_action(void *args) {
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

  hpx_addr_t gDone   = hpx_lco_and_new(HPX_LOCALITIES);
  hpx_call(grid, _initialize, NULL, 0, gDone);
  hpx_lco_wait(gDone);
  hpx_lco_delete(gDone, HPX_NULL);

  hpx_addr_t nDone   = hpx_lco_and_new(HPX_LOCALITIES);
  hpx_call(new_grid, _initialize, NULL, 0, nDone);
  hpx_lco_wait(nDone);
  hpx_lco_delete(nDone, HPX_NULL);

  update_grid();

  hpx_gas_free(grid, HPX_NULL);
  hpx_gas_free(new_grid, HPX_NULL);

  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_initialize, _initialize_action);
  HPX_REGISTER_ACTION(&_write_double, _write_double_action);
  HPX_REGISTER_ACTION(&_read_double, _read_double_action);
  HPX_REGISTER_ACTION(&_stencil, _stencil_action);
  HPX_REGISTER_ACTION(&_spawn_stencil, _spawn_stencil_action);
  HPX_REGISTER_ACTION(&_row_stencil, _row_stencil_action);
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

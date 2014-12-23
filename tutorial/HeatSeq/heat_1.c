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

#include "heat_1.h"

static hpx_action_t _main      = 0;
static hpx_action_t _evolve    = 0;

/* helper functions */
static void _usage(FILE *stream) {
  fprintf(stream, "Usage: Heat Sequence [options]\n"
          "\t-n, number of domains, nDoms\n"
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


void problem_init(Domain *ld) {
  ld->grid_points = calloc(1, sizeof(grid_storage_t));
  assert(ld->grid_points != NULL);

  int nptr_array = ((N+2)*(N+2))/HPX_LOCALITIES;
  ld->grid_points->array = calloc(nptr_array, sizeof(grid_point_t));
  assert(ld->grid_points->array != NULL);

  for (int i = 0; i < N + 2; i++) {
    for (int j = 0; j < N + 2; j++) {
      ld->grid_points->array->grid[i][j] = 0.0;
      ld->grid_points->array->new_grid[i][j] = 0.0;
    }
  }

  /* Heat one side of the solid */
  for (int j = 1; j < N + 2; j++) {
    ld->grid_points->array->grid[0][j] = 1.0;
    ld->grid_points->array->new_grid[0][j] = 1.0;
  }
}

static hpx_action_t write_double = 0;

static int write_double_action(double *d) {
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_pin(target, &addr)) {
    return HPX_RESEND;
  }
  *addr = d[0];
  hpx_gas_unpin(target);
  hpx_thread_continue(sizeof(double), &d[1]);
}

static hpx_action_t read_double = 0;

static int read_double_action(void *unused) {
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_pin(target, (void**)&addr)) {
    return HPX_RESEND;
  }
  double d = *addr;
  hpx_gas_unpin(target);
  hpx_thread_continue(sizeof(d), d);
}

static int offset_of(int i, int j) {
  return (i * (N + 2) * sizeof(double)) + (j * sizeof(double));
}

static hpx_action_t stencil = 0;

static int stencil_action(int *ij) {
  // read the value in this cell
  hpx_addr_t target = hpx_thread_current_target();
  double *addr = NULL;
  if (!hpx_gas_pin(target, (void**)&addr)) {
    return HPX_RESEND;
  }
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
    vals + 0,
    vals + 1,
    vals + 2,
    vals + 3
  };

  hpx_addr_t futures[4] = {
    hpx_future_new(sizeof(double)),
    hpx_future_new(sizeof(double)),
    hpx_future_new(sizeof(double)),
    hpx_future_new(sizeof(double))
  };

  size_t sizes[4] = {
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
  }

  for (int i = 0; i < 4; ++i) {
    hpx_call(neighbors[i], read_double, NULL, 0, futures[i]);
  }

  hpx_lco_get_all(4, futures, sizes, addrs, NULL);

  for (int n = 0; n < 4; ++i) {
    hpx_lco_delete(futures[n]);
  }

  // compute the new T and dT
  double T = 0.25 * (vals[0] + vals[1] + vals[2] + vals[3]); // stencil
  double dT = T - v; // local variation

  // write out the new T and continue the dT for the min reduction
  double cont_args[2] = { T, fabs(dT) };
  hpx_addr_t new_grid_addr = hpx_addr_add(new_grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(new_grid_addr, write_double, cont_args, sizeof(cont_args), NULL,
              NULL);
}

static hpx_action_t spawn_stencil = 0;

static void spawn_stencil_args_init(void *out, const int j, const void *env) {
  int *ij = out;
  int *i = env;
  ij[0] = *i;
  ij[1] = j;
}

static int spawn_stencil_action(int *ij) {
  int i = ij[0];
  int j = ij[1];
  hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
  hpx_call_cc(cell, stencil, ij, 2*sizeof(int), NULL, NULL);
}

static void row_stencil_args_init(void *out, const int i, const void *env) {
  *((int*)out) = i;
}

static int row_stencil_action(int *i) {
  row_stencil_args_t *args = out;
  hpx_par_call_sync(spawn_stencil, 1, N + 1, 1, N + 2, 2 * sizeof(int),
                    spawn_stencil_args_init, sizeof(int), i);
}

void update_grid(Domain *ld)
{
  struct timeval ts_st, ts_end;
  double time;
  double dT, dTmax, epsilon;
  int finished;
  int nr_iter;

  /* Set the precision wanted */
  epsilon  = 0.0001;
  finished = 0;
  nr_iter = 0;

  gettimeofday( &ts_st, NULL );

  do {
    dTmax = 0.0;

    hpx_addr_t min = hpx_lco_allreduce_new(N * N, 1, sizeof(dTmax), min_double, init_double);
    // for (int i = 1; i < N + 1; i++) {
    //   for (int j = 1; j < N + 1; j++) {
    //     int args[2] = { i, j };
    //     hpx_addr_t cell = hpx_addr_add(grid, offset_of(i, j), BLOCKSIZE);
    //     hpx_call(cell, stencil, args, sizeof(args), min);
    //   }
    // }
    hpx_par_call(row_stencil, 1, N + 1, 1, N + 2, sizeof(row_stencil_args_t),
                 row_stencil_args_init, 0, NULL, min);
    int e = hpx_lco_get(min, sizeof(dTmax), &dTMax);
    dbg_check(e, "failed to get min");



    if (dTmax < epsilon ) // is the precision reached good enough ?
      finished = 1;
    else {
      for (int k = 0; k < N + 2; k++)      // not yet  Need to prepare
        for (int l = 0; l < N + 2; l++)    // ourselves for doing a new
          ld->grid_points->array->grid[k][l] = ld->grid_points->array->new_grid[k][l];
    }
    nr_iter++;
  } while (finished == 0);

  gettimeofday(&ts_end, NULL);

  /* compute the execution time */
  time = ts_end.tv_sec + (ts_end.tv_usec / 1000000.0);
  time -= ts_st.tv_sec + (ts_st.tv_usec / 1000000.0);

  if (ld->myIndex == 0) {
    printf("%d iterations in %.3lf sec\n", nr_iter, time ); /* and prints it */
  }
}

static int _evolve_action(const InitArgs *init) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->nDoms = init->nDoms;
  ld->myIndex = init->index;

  problem_init(ld);
  update_grid(ld);

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(int *input)
{
  int nDoms, nx, maxcycles, k;
  nDoms = input[0];
  nx = input[1];
  maxcycles = input[2];

  hpx_addr_t domain = hpx_gas_global_alloc(nDoms, sizeof(Domain));
  hpx_addr_t complete = hpx_lco_and_new(nDoms);

  for (k = 0; k < nDoms; k++) {
    InitArgs args = {
      .index = k,
      .nDoms = nDoms,
      .cores = nx,
      .maxcycles = maxcycles,
    };
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k, sizeof(Domain));
    hpx_call(block, _evolve, &args, sizeof(args), complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);

  hpx_gas_free(domain, HPX_NULL);
  hpx_shutdown(HPX_SUCCESS);
}

/**
 * Registers functions as actions.
 */
void _register_actions(void) {
  /* register action for parcel (must be done by all ranks) */
  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_evolve, _evolve_action);
}

// Main routine
int main(int argc, char *argv[])
{
  int nDoms, nx, maxcycles;
  // default
  nDoms = 8;
  nx    = 15;
  maxcycles = 10;

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
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
  input[1] = nx;
  input[2] = maxcycles;
  printf("Number of domains: %d maxcycles: %d, nx = %d\n", nDoms,
           maxcycles, nx);

  e = hpx_run(&_main, input, 3*sizeof(int));
  return e;
}

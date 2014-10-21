#ifndef LULESH_HPX_H
#define LULESH_HPX_H

#include <limits.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include "hpx/hpx.h"
#include "hpx/future.h"

#define n_dim    1 // problem dimensionality
#define np       6  // order of wavelets/points in Lagrange interpolation
#define ns_x     10 // number of points in x-direction of the base grid
#define ns_y     0
#define ns_z     0 
#define JJ       19  // maximum number of resolution levels to use
#define n_variab 3 // number of primary variables
#define n_aux    6 // number of auxiliary variables
#define n_deriv  6 // number of derivatives needed in the right-hand side
#define nstn     5 // number of elements in the derivative stencil 
#define n_rhs    3 // number of right hand side functions
#define n_gen   4 // number of buffers for the RK4 integrator
#define n_neighbors 2

#define HASH_TBL_SIZE 6291469

typedef enum {
  essential    = 3,
  neighboring  = 2,
  nonessential = 1,
  uninitialized = 0
} coll_status_t;

typedef struct coll_point_t {
  double u[n_gen][n_variab + n_aux];
  double du[n_gen][n_deriv];
  double rhs[n_gen][n_rhs];
  double coords[n_dim];
  int index[n_dim];
  int level;
  coll_status_t status[2];
  int wavelet_stencil[n_dim][np][n_dim]; // [dir][jth][indices]
  int wavelet_coef[n_dim];
  int derivative_stencil[n_dim][nstn][n_dim]; // [dir][jth][indices]
  int derivative_coef[n_dim];
  int stage[n_gen];
  int parent[n_dim]; // index of the parent
  int neighbors[n_neighbors][n_dim]; // indices of the neighbors
  double stamp; // latest time stamp
} coll_point_t;

typedef struct hash_entry_t{
  coll_point_t *point; ///< pointer to the collocation point
  uint64_t mkey;  ///< morton key for this entry
  struct hash_entry_t *next; ///< pointer to the next entry
} hash_entry_t;

typedef struct {
  coll_point_t *array; ///< storage for zeroth and first levels
  hash_entry_t *hash_table[HASH_TBL_SIZE]; ///< storage for higher levels
} wamr_storage_t;

typedef struct {
  int           index;
  int           nDoms;
  int       maxcycles;
  int           cores;
} InitArgs;

typedef struct Domain {
  int nDoms;
  int myindex;

  double e_r;
  double c_r;
  double a_r;
  double t0;
  double tf;
  double eps[n_variab];

  double L_dim[n_dim];
  double G;
  double Jac[n_dim][n_dim];
  
  double Gamma_r;
  double Prr;
  double p_r;
  double mur;
  double k_r;
  double c_v;

  double lag_coef[np - 1][np];
  double nfd_diff_coeff[2][nstn][nstn];
  wamr_storage_t *coll_points;
  int max_level;
} Domain;

void problem_init(Domain *);
double compute_numer_1st(const int ell, const int j, const int p);
double compute_numer_2nd(const int ell, const int j, const int p);
double compute_denom(const int j, const int p);
void set_lagrange_deriv_coef(Domain *);
void set_lagrange_coef(Domain *);
void storage_init(Domain *);
void create_full_grids(Domain *ld);
void initial_condition(const double coords[n_dim], double *u,Domain *ld);
int get_stencil_type(const int myorder, const int range);
void get_stencil_indices(const int myindex, const int index_range,
                         const int step, int indices[np]);
void forward_wavelet_trans(const coll_point_t *point, const char type,
                           const int *mask, const int gen, double *approx,Domain *ld);
coll_point_t *get_coll_point(const int index[n_dim],Domain *ld);
uint64_t morton_key(const int index[n_dim]);
uint64_t hash(const uint64_t k);
void create_neighboring_point(coll_point_t *essen_point, const double stamp,Domain *ld);
void create_adap_grids(Domain *ld);
void advance_time_stamp(coll_point_t *point, const double stamp,
                        const int gen,Domain *ld); 
void create_nonessential_point(coll_point_t *nonessen_point,
                               const int index[n_dim], const double stamp,Domain *ld);
coll_point_t *add_coll_point(const int index[n_dim], int *flag,Domain *ld);
int get_level(const int index);
void deriv_stencil_config(const double stamp,Domain *ld); 
void deriv_stencil_helper(coll_point_t *point, const int dir, const double stamp,Domain *ld);
void get_deriv_stencil(coll_point_t *point, const int dir, const double stamp,Domain *ld);
coll_point_t *get_closest_point(coll_point_t *point, const int dir, const int type,Domain *ld); 
double get_global_dt(Domain *ld); 
void global_dt_helper(coll_point_t *point, double *dt,Domain *ld);
double get_local_dt(const coll_point_t *point,Domain *ld);
void apply_time_integrator(const double t, const double dt,Domain *ld);
void rhs_nonessen_stage3(coll_point_t *point, const int gen,Domain *ld);
void rhs_active_stage3(coll_point_t *point, const int gen,Domain *ld);
void rhs_nonessen_stage2(coll_point_t *point, const int gen,Domain *ld);
void rhs_active_stage2(coll_point_t *point, const int gen,Domain *ld);
void rhs_nonessen_stage1(coll_point_t *point, const int gen,Domain *ld);
void rhs_active_stage1(coll_point_t *point, const int gen,Domain *ld);
void compute_rhs(const double t, const int gen,Domain *ld);
void rk4_helper4(coll_point_t *point, const double dt);
void rk4_helper3(coll_point_t *point, const double dt);
void rk4_helper2(coll_point_t *point, const double dt);
void rk4_helper1(coll_point_t *point, const double dt);
void integrator_helper(coll_point_t *point, const double dt, void(*func)(coll_point_t *, const double),Domain *ld);
void rhs_helper(coll_point_t *point, const int gen,
                void(*func)(coll_point_t *, const int,Domain *ld),Domain *ld);
void deriv_nonessen_helper2(coll_point_t *point, const int gen,Domain *ld);
void deriv_nonessen_helper1(coll_point_t *point, const int stage, const int gen,
                            void(*func)(coll_point_t *, const int,Domain *ld),Domain *ld);
void stage_reset_helper(coll_point_t *point,Domain *ld);
double get_deriv(const int ivar, const int dir, const int gen, const int order, coll_point_t *point,Domain *ld);

#endif

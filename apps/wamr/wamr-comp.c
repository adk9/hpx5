
#include "wamr-hpx.h"

void problem_init(Domain *ld) {

  ld->Gamma_r = 1.4;
  ld->Prr = 1.0;
  ld->p_r = 1.0;
  ld->mur = 1.813181363584554e-4;
  ld->k_r = 2618.72719838725;
  ld->c_v = 7410690.78129267;

  ld->e_r = ld->Prr / (ld->Gamma_r - 1) / ld->p_r; // reference total energy/unit mass
  ld->a_r = sqrt(ld->Gamma_r * ld->Prr / ld->p_r);   // reference acoustic speed  
  ld->c_r = 0.0; // reference sound speed
  ld->e_r += 0.5 * ld->c_r * ld->c_r;
  ld->t0 = 0.0;
  ld->tf = 1;

  // first set up eps with 'eps_scale' in the fortran code
  ld->eps[0] = ld->p_r; // [0] = density
  for (int dir = 1; dir <= n_dim; dir++) // [1 : n_dim] = momentum
    ld->eps[dir] = ld->p_r * ld->a_r;
  ld->eps[n_dim + 1] = fabs(ld->e_r * ld->p_r); // [n_dim + 1] = total energy/unit volume

  // now set up scaled version of the eps
  for (int i = 0; i < n_variab; i++)
    ld->eps[i] *= 1e-3;

  printf("eps: %15.14e %15.14e % 15.14e\n", ld->eps[0], ld->eps[1], ld->eps[2]);

  ld->L_dim[0] = 1.0; // domain length [cm] 
  ld->G = ld->L_dim[0]; // set Jacobian
  ld->Jac[0][0] = 1.0 / ld->L_dim[0];

  set_lagrange_coef(ld);
  set_lagrange_deriv_coef(ld);
}

/// @brief Generate Lagrange interpolation coefficients
/// ---------------------------------------------------------------------------
void set_lagrange_coef(Domain *ld) {
  // Generate the coefficients using symmetry 
  double *ptr1 = &(ld->lag_coef[0][0]);
  double *ptr2 = &(ld->lag_coef[np - 2][np - 1]);

  for (int i = 0; i < np / 2 - 1; i++) {
    double x = i + 0.5;
    for (int j = 0; j < np; j++) {
      double coef = 1.0;
      for (int k = 0; k < j; k++) {
        coef *= (x - k) / (j - k);
      }

      for (int k = j + 1; k < np; k++) {
        coef *= (x - k) / (j - k);
      }

      *ptr1 = coef;
      *ptr2 = coef;

      ptr1++;
      ptr2--;
    }
  }

  // ptr1 points to lag_coef[np/2 - 1][0]
  // ptr2 points to lag_coef[np/2 - 1][np - 1]
  // need to compute np/2 more coefficients
  double x = (np - 1.0) / 2.0;
  for (int j = 0; j < np / 2; j++) {
    double coef = 1.0;
    for (int k = 0; k < j; k++) {
      coef *= (x - k) / (j - k);
    }

    for (int k = j + 1; k < np; k++) {
      coef *= (x - k) / (j - k);
    }

    *ptr1 = coef;
    *ptr2 = coef;

    ptr1++;
    ptr2--;
  }
}

/// ---------------------------------------------------------------------------
/// @brief Precompute coefficients of the 1st and 2nd order derivatives of the 
///        Lagrange interpolation polynomial
/// ---------------------------------------------------------------------------
void set_lagrange_deriv_coef(Domain *ld) {
  // coefficient for stencil of size nstn
  for (int i = 0; i < nstn; i++) { // i is x_i 
    for (int j = 0; j < nstn; j++) { // j is L_j
      double denom = compute_denom(j, nstn);
      double numer1 = compute_numer_1st(i, j, nstn);
      double numer2 = compute_numer_2nd(i, j, nstn);

      ld->nfd_diff_coeff[0][i][j] = numer1 / denom;
      ld->nfd_diff_coeff[1][i][j] = numer2 / denom;
    }
  }
}

/// ---------------------------------------------------------------------------
/// @brief Evalute the denominator of the jth Lagrange interpolation polynomial
/// ---------------------------------------------------------------------------
double compute_denom(const int j, const int p) {
  double retval = 1.0;

  for (int loop = 0; loop < j; loop++)
    retval *= (j - loop);

  for (int loop = j + 1; loop < p; loop++)
    retval *= (j - loop);

  return retval;
}

/// ---------------------------------------------------------------------------
/// @brief Evaluate the numerator of the 1st order derivative of the jth 
///        Lagrange interpolation polynomial of order p at point ell
/// --------------------------------------------------------------------------
double compute_numer_1st(const int ell, const int j, const int p) {
  double numer = 0;
  for (int m = 0; m < p; m++) {
    if (m != j) {
      double mul = 1.0;
      for (int k = 0; k < p; k++) {
        if (k != m && k != j) {
          mul *= (ell - k);
        }
      }
      numer += mul;
    }
  }

  return numer;
}

/// ---------------------------------------------------------------------------
/// @brief Evaluate the numerator of the 2nd order derivative of the jth  
///        Lagrange interpolation polynomial of order p at point ell
/// ---------------------------------------------------------------------------
double compute_numer_2nd(const int ell, const int j, const int p) {
  double numer = 0;
  for (int m = 0; m < p; m++) {
    if (m != j) {
      for (int n = 0; n < p; n++) {
        if (n != j && n != m) {
          double mul = 1.0;
          for (int k = 0; k < p; k++) {
            if (k != j && k != n && k != m) {
              mul *= (ell - k);
            }
          }
          numer += mul;
        }
      }
    }
  }

  return numer;
}

int _cfg_action(Cfg_action_helper *ld)
{
  hpx_addr_t local = hpx_thread_current_target();
  coll_point_t *point = NULL;
  if (!hpx_gas_try_pin(local, (void**)&point))
    return HPX_RESEND;

  int i = ld->i;

  int i2 = i / 2;
  int ihalf = i % 2;
  point->coords[0] = ld->L_dim[0] * i / 2 / ns_x;
  point->index[0] = (i2 + 0.5 * ihalf) * ld->step_size;
  initial_condition(point->coords, point->u[0],ld->Prr,ld->p_r,ld->Gamma_r);
  point->status[0] = ihalf ? neighboring : essential;
  point->level = ihalf;
  point->stamp = ld->t0;
  point->parent[0] = -1;
  for (int j = 0; j < n_neighbors; j++) {
    for (int dir = 0; dir < n_dim; dir++) {
      point->neighbors[j][dir] = -1;
    }
  }
  hpx_gas_unpin(local);
}

void create_full_grids(Domain *ld) {
  //assert(ld->coll_points != NULL);
  const int step_size = 1 << JJ;
  ld->max_level = 1;

  int mask[n_variab + n_aux] = {0};
  for (int ivar = 0; ivar < n_variab; ivar++)
    mask[ivar] = 1;

  Cfg_action_helper cfg[ns_x*2+1]; 
  hpx_addr_t complete = hpx_lco_and_new(ns_x*2+1);
  int j;
  for (int i = 0; i <= ns_x * 2; i++) {
    cfg[i].i = i; 
    for (j=0;j<n_dim;j++) {
      cfg[i].L_dim[j] = ld->L_dim[j];
    }
    cfg[i].Prr = ld->Prr;
    cfg[i].p_r = ld->p_r;
    cfg[i].Gamma_r = ld->Gamma_r;
    cfg[i].t0 = ld->t0;
    hpx_addr_t there = hpx_addr_add(ld->basecollpoints,i*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_call(there,_cfg,&cfg[i],sizeof(Cfg_action_helper),complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete,HPX_NULL);
#if 0
  for (int i = 0; i <= ns_x * 2; i++) {
      int i2 = i / 2;
      int ihalf = i % 2;
      coll_point_t *point = &(ld->coll_points->array[i]);
      if (ihalf) { // point is level 1, configure its wavelet stencil
        point->parent[0] = ld->coll_points->array[i - 1].index[0];
        int indices[np] = {0};
        point->wavelet_coef[0] = get_stencil_type(i2, ns_x - 1);
        get_stencil_indices(i, 2 * ns_x, 2, indices);
        for (int j = 0; j < np; j++)
          point->wavelet_stencil[0][j][0] =
            ld->coll_points->array[indices[j]].index[0];

        // perform wavelet transform
        double approx[n_variab + n_aux] = {0};
        forward_wavelet_trans(point, 'u', mask, 0, approx,ld);
        for (int ivar = 0; ivar < n_variab; ivar++) {
          if (fabs(point->u[0][ivar] - approx[ivar]) >= ld->eps[ivar]) {
            point->status[0] = essential;
            break;
          }
        }
      } else {
        point->wavelet_coef[0] = -1; // -1 means wavelet transform not needed
        point->parent[0] = -1; // no parent 
        if (i >= 1)
          point->neighbors[0][0] = ld->coll_points->array[i - 1].index[0];
  
        if (i <= ns_x * 2 - 1)
          point->neighbors[1][0] = ld->coll_points->array[i + 1].index[0];
      }
  }
#endif
}

void initial_condition(const double coords[n_dim], double *u,double Prr,double p_r,double Gamma_r) {
  double Pra = 0.1;
  double p_ra = 0.125;
  double thk = 1.0e-2;
  double x0 = 0.5;
  double x = coords[0];
  double Prh = 0.5*((Prr + Pra) - (Prr - Pra) * tanh((x - x0) / thk));
  double p_h = 0.5*((p_r + p_ra) - (p_r - p_ra) * tanh((x - x0) / thk));
  double e_h = Prh / (Gamma_r - 1) / p_h;

  u[0] = p_h;
  u[1] = 0.0;
  u[2] = p_h * e_h;
}


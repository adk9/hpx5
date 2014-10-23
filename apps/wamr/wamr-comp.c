
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

  return HPX_SUCCESS;
}

int _cfg2_action(Cfg_action_helper2 *ld)
{
  hpx_addr_t local = hpx_thread_current_target();
  coll_point_t *point = NULL;
  if (!hpx_gas_try_pin(local, (void**)&point))
    return HPX_RESEND;

  int i = ld->i;

  int i2 = i / 2;
  int ihalf = i % 2;
  if (ihalf) { // point is level 1, configure its wavelet stencil
    hpx_addr_t there = hpx_addr_add(ld->basecollpoints,(i-1)*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_addr_t complete = hpx_lco_and_new(1);
    coll_point_t *parent = malloc(sizeof(*parent));
    hpx_gas_memget(parent,there,sizeof(coll_point_t),complete);

    int indices[np] = {0};
    get_stencil_indices(i, 2 * ns_x, 2, indices);

    hpx_lco_wait(complete);
    hpx_lco_delete(complete,HPX_NULL);
    point->parent[0] = parent->index[0];
    free(parent);

    coll_point_t **pnts = malloc(np*sizeof(*pnts));
    for (int j = 0; j < np; j++) {
      pnts[j] = malloc(sizeof(**pnts));
    }
    hpx_addr_t finish = hpx_lco_and_new(np);
    for (int j = 0; j < np; j++) {
      hpx_addr_t there = hpx_addr_add(ld->basecollpoints,indices[j]*sizeof(coll_point_t),sizeof(coll_point_t));
      hpx_gas_memget(pnts[j],there,sizeof(coll_point_t),finish);
    }
    hpx_lco_wait(finish);
    hpx_lco_delete(finish,HPX_NULL);

    for (int j = 0; j < np; j++) { 
      point->wavelet_stencil[0][j][0] = pnts[j]->index[0];
    }

    for (int j = 0; j < np; j++) 
      free(pnts[j]);
    free(pnts);
    
    // perform wavelet transform
    double approx[n_variab + n_aux] = {0};
    forward_wavelet_trans(point, 'u', ld->mask, 0, approx,ld->basecollpoints,ld->collpoints,ld->lag_coef);
    for (int ivar = 0; ivar < n_variab; ivar++) {
      if (fabs(point->u[0][ivar] - approx[ivar]) >= ld->eps[ivar]) {
        point->status[0] = essential;
        break;
      }
    }
  } else {
    point->wavelet_coef[0] = -1; // -1 means wavelet transform not needed
    point->parent[0] = -1; // no parent 
    if (i >= 1) {
      hpx_addr_t there = hpx_addr_add(ld->basecollpoints,(i-1)*sizeof(coll_point_t),sizeof(coll_point_t));
      hpx_addr_t complete = hpx_lco_and_new(1);
      coll_point_t *parent = malloc(sizeof(*parent));
      hpx_gas_memget(parent,there,sizeof(coll_point_t),complete);
      hpx_lco_wait(complete);
      hpx_lco_delete(complete,HPX_NULL);
      point->neighbors[0][0] = parent->index[0];
      free(parent);
    }

    if (i <= ns_x * 2 - 1) {
      hpx_addr_t there = hpx_addr_add(ld->basecollpoints,(i+1)*sizeof(coll_point_t),sizeof(coll_point_t));
      hpx_addr_t complete = hpx_lco_and_new(1);
      coll_point_t *parent = malloc(sizeof(*parent));
      hpx_gas_memget(parent,there,sizeof(coll_point_t),complete);
      hpx_lco_wait(complete);
      hpx_lco_delete(complete,HPX_NULL);
      point->neighbors[1][0] = parent->index[0];
      free(parent);
    }
  }

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

void create_full_grids(Domain *ld) {
  //assert(ld->coll_points != NULL);
  const int step_size = 1 << JJ;
  ld->max_level = 1;

  Cfg_action_helper cfg[ns_x*2+1]; 
  hpx_addr_t complete = hpx_lco_and_new(ns_x*2+1);
  int j,k;
  for (int i = 0; i <= ns_x * 2; i++) {
    cfg[i].i = i; 
    for (j=0;j<n_dim;j++) {
      cfg[i].L_dim[j] = ld->L_dim[j];
    }
    cfg[i].Prr = ld->Prr;
    cfg[i].p_r = ld->p_r;
    cfg[i].Gamma_r = ld->Gamma_r;
    cfg[i].t0 = ld->t0;
    cfg[i].step_size = step_size;
    hpx_addr_t there = hpx_addr_add(ld->basecollpoints,i*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_call(there,_cfg,&cfg[i],sizeof(Cfg_action_helper),complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete,HPX_NULL);

  hpx_addr_t complete2 = hpx_lco_and_new(ns_x*2+1);
  Cfg_action_helper2 cfg2[ns_x*2+1];
  for (int i = 0; i <= ns_x * 2; i++) {
    cfg2[i].i = i; 
    for (int ivar = 0; ivar < n_variab; ivar++) {
      cfg2->mask[ivar] = 1;
      cfg2->eps[ivar] = ld->eps[ivar];
    }
    for (int ivar = n_variab; ivar < n_variab+n_aux; ivar++)
      cfg2->mask[ivar] = 0;

    for (j=0;j<np-1;j++) {
      for (k=0;k<np;k++) {
        cfg2[i].lag_coef[j][k] = ld->lag_coef[j][k];
      }
    }
    cfg2[i].basecollpoints = ld->basecollpoints;
    cfg2[i].collpoints = ld->collpoints;

    hpx_addr_t there = hpx_addr_add(ld->basecollpoints,i*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_call(there,_cfg2,&cfg2[i],sizeof(Cfg_action_helper2),complete2);
  }
  hpx_lco_wait(complete2);
  hpx_lco_delete(complete2,HPX_NULL);
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

/// @brief Determines the indices of points used by the interpolation stencil
/// ---------------------------------------------------------------------------
void get_stencil_indices(const int myindex, const int index_range,
                         const int step, int indices[np]) {
  int start;
  int temp = (np - 1) * step / 2;

  if (myindex - temp < 0) {
    start = 0;
  } else if (myindex + temp > index_range) {
    start = index_range - step * (np - 1);
  } else {
    start = myindex - temp;
  }

  for (int i = 0; i < np; i++)
    indices[i] = start + i * step;
}

/// Forward wavelet transform
/// ---------------------------------------------------------------------------
void forward_wavelet_trans(const coll_point_t *point, const char type,
                           const int *mask, const int gen, double *approx,hpx_addr_t basecollpoints,hpx_addr_t collpoints,double lag_coef[np - 1][np]) {
  assert(type == 'u' || type == 'd');
  coll_point_t *wavelet_stencil[np] = {NULL};
  double *coef = NULL;

  // In general, a point may have multiple interpolation direction to complete
  // the transformation. If the data is distributed, one would ideally choose
  // the direction based on the amount of local data. The current version,
  // however, will just choose the first direction in which interpolation is
  // required. 
  for (int dir = 0; dir < n_dim; dir++) {
    int interp_case = point->wavelet_coef[dir];
    if (interp_case != -1) {
      coef = &(lag_coef[interp_case][0]);
      for (int j = 0; j < np; j++) 
        wavelet_stencil[j] = get_coll_point(point->wavelet_stencil[dir][j],basecollpoints,collpoints);
    }
  }

  if (type == 'u') {
    int nvar = n_variab + n_aux; // length of the mask
    for (int ivar = 0; ivar < nvar; ivar++)
      approx[ivar] = 0;

    for (int i = 0; i < np; i++) {
      double *ui = &wavelet_stencil[i]->u[gen][0];
      for (int ivar = 0; ivar < nvar; ivar++) 
        approx[ivar] += coef[i] * ui[ivar] * mask[ivar];
    }
    for (int i = 0; i < np; i++) {
      free(wavelet_stencil[i]);
    }
  } else {
    int nvar = n_deriv;
    for (int ivar = 0; ivar < nvar; ivar++)
      approx[ivar] = 0;

    for (int i = 0; i < np; i++) {
      double *dui = &wavelet_stencil[i]->du[gen][0];
      for (int ivar = 0; ivar < nvar; ivar++)
        approx[ivar] += coef[i] * dui[ivar] * mask[ivar];
    }
    for (int i = 0; i < np; i++) {
      free(wavelet_stencil[i]);
    }
  }
}

/// @brief Retrieve collocation point based on its Morton key  
/// ---------------------------------------------------------------------------
coll_point_t *get_coll_point(const int index[n_dim],hpx_addr_t basecollpoints,hpx_addr_t collpoints) {
  static const int step = 1 << (JJ - 1);
  coll_point_t *retval = NULL;

  bool stored_in_array = true;
  for (int dir = 0; dir < n_dim; dir++)
    stored_in_array &= (index[dir] % step == 0);

  if (stored_in_array) {
   // retval = &(ld->coll_points->array[index[0] / step]);

    hpx_addr_t there = hpx_addr_add(basecollpoints,(index[0] / step)*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_addr_t complete = hpx_lco_and_new(1);
    coll_point_t *point = malloc(sizeof(*point));
    hpx_gas_memget(point,there,sizeof(coll_point_t),complete);
    hpx_lco_wait(complete);
    hpx_lco_delete(complete,HPX_NULL);
    return point;
  } else {
    printf(" PROBLEM!!!!\n");
/*
    uint64_t mkey = morton_key(index);
    uint64_t hidx = hash(mkey);
    hash_entry_t *curr = ld->coll_points->hash_table[hidx];
    while (curr != NULL) {
      if (curr->mkey == mkey) {
        retval = curr->point;
        break;
      }
      curr = curr->next;
    }
 */
  }

  return retval;
}



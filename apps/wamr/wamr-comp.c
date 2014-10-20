
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

void storage_init(Domain *ld) {
  ld->coll_points = calloc(1, sizeof(wamr_storage_t));
  assert(ld->coll_points != NULL);
  // number of collocation points stored in the array
  int npts_array = (2 * ns_x + 1) * (2 * ns_y + 1) * (2 * ns_z + 1);
  ld->coll_points->array = calloc(npts_array, sizeof(coll_point_t));
  assert(ld->coll_points->array != NULL);
}

void create_full_grids(Domain *ld) {
  assert(ld->coll_points != NULL);
  const int step_size = 1 << JJ;
  ld->max_level = 1;

  int nDoms = ld->nDoms;
  int myindex = ld->myindex;

  int mask[n_variab + n_aux] = {0};
  for (int ivar = 0; ivar < n_variab; ivar++)
    mask[ivar] = 1;

  for (int i = 0; i <= ns_x * 2; i++) {
    if ( i%nDoms == myindex ) {
      int i2 = i / 2;
      int ihalf = i % 2;
      coll_point_t *point = &(ld->coll_points->array[i]);
      point->coords[0] = ld->L_dim[0] * i / 2 / ns_x;
      point->index[0] = (i2 + 0.5 * ihalf) * step_size;
      initial_condition(point->coords, point->u[0],ld);
      point->status[0] = ihalf ? neighboring : essential;
      point->level = ihalf;
      point->stamp = ld->t0;
      point->parent[0] = -1;
      for (int j = 0; j < n_neighbors; j++) {
        for (int dir = 0; dir < n_dim; dir++) {
          point->neighbors[j][dir] = -1;
        }
      }
    }
  }

  for (int i = 0; i <= ns_x * 2; i++) {
    if ( i%nDoms == myindex ) {
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
  }
}

void initial_condition(const double coords[n_dim], double *u,Domain *ld) {
  double Pra = 0.1;
  double p_ra = 0.125;
  double thk = 1.0e-2;
  double x0 = 0.5;
  double x = coords[0];
  double Prh = 0.5*((ld->Prr + Pra) - (ld->Prr - Pra) * tanh((x - x0) / thk));
  double p_h = 0.5*((ld->p_r + p_ra) - (ld->p_r - p_ra) * tanh((x - x0) / thk));
  double e_h = Prh / (ld->Gamma_r - 1) / p_h;

  u[0] = p_h;
  u[1] = 0.0;
  u[2] = p_h * e_h;
}

/// @brief Determines which row in lag_coef to use by the stencil point
/// ---------------------------------------------------------------------------
int get_stencil_type(const int myorder, const int range) {
  int stencil_type;

  if (myorder < np / 2 - 1) {
    stencil_type = myorder;
  } else if ((range - myorder) < np / 2 - 1) {
    stencil_type = (np - 2) - (range - myorder);
  } else {
    stencil_type = np / 2 - 1;
  }

  return stencil_type;
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
                           const int *mask, const int gen, double *approx,Domain *ld) {
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
      coef = &(ld->lag_coef[interp_case][0]);
      for (int j = 0; j < np; j++)
        wavelet_stencil[j] = get_coll_point(point->wavelet_stencil[dir][j],ld);
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
  } else {
    int nvar = n_deriv;
    for (int ivar = 0; ivar < nvar; ivar++)
      approx[ivar] = 0;

    for (int i = 0; i < np; i++) {
      double *dui = &wavelet_stencil[i]->du[gen][0];
      for (int ivar = 0; ivar < nvar; ivar++)
        approx[ivar] += coef[i] * dui[ivar] * mask[ivar];
    }
  }
}

/// @brief Retrieve collocation point based on its Morton key  
/// ---------------------------------------------------------------------------
coll_point_t *get_coll_point(const int index[n_dim],Domain *ld) {
  static const int step = 1 << (JJ - 1);
  coll_point_t *retval = NULL;

  bool stored_in_array = true;
  for (int dir = 0; dir < n_dim; dir++)
    stored_in_array &= (index[dir] % step == 0);

  if (stored_in_array) {
    retval = &(ld->coll_points->array[index[0] / step]);
  } else {
    uint64_t mkey = morton_key(index);
    uint64_t hidx = hash(mkey);
    hash_entry_t *curr = &(ld->coll_points->hash_table[hidx]);
    while (curr != NULL) {
      if (curr->mkey == mkey) {
        retval = curr->point;
        break;
      }
      curr = curr->next;
    }
  }

  return retval;
}

/// @brief Computes the morton key for the given indices.
/// ----------------------------------------------------------------------------
uint64_t morton_key(const int index[n_dim]) {
  uint64_t key = index[0];
  return key;
}

/// @brief Compute the hash key from the morton key.
/// @param [in] k The Morton key
/// @returns The hash key
/// ----------------------------------------------------------------------------
uint64_t hash(const uint64_t k) {
  return (k % HASH_TBL_SIZE);
}




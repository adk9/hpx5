
#include "wamr-hpx.h"

const int off_x[n_neighbors] = {-1, 1};
const int diag[n_dim * 2] = {1, 0}; // +x, -x

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

  int mask[n_variab + n_aux] = {0};
  for (int ivar = 0; ivar < n_variab; ivar++)
    mask[ivar] = 1;

  for (int i = 0; i <= ns_x * 2; i++) {
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
    hash_entry_t *curr = ld->coll_points->hash_table[hidx];
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

/// @brief Create adaptive grid 
/// ----------------------------------------------------------------------------
void create_adap_grids(Domain *ld) {
  for (int i = 1; i <= ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      create_neighboring_point(point, ld->t0,ld);
  }
}

/// ----------------------------------------------------------------------------
/// @brief Add neighboring point around an essential point
/// ----------------------------------------------------------------------------
void create_neighboring_point(coll_point_t *essen_point, const double stamp,Domain *ld) {
  // set up mask for wavelet transform
  int mask[n_variab + n_aux] = {0}; 
  for (int ivar = 0; ivar < n_variab; ivar++) 
    mask[ivar] = 1;

  // assert that the refinement does not exceed prescribed limit
  assert(essen_point->level < JJ); 

  // spacing between grid points of level 0 
  int step0 = 1 << JJ; 

  // level of the essential points where neighboring points are added 
  int level = essen_point->level; 

  // spacing between grid points at the level of the essential point 
  int step_e = 1 << (JJ - level); 

  // spacing between grid points at the level of the neighboring point 
  int step_n = step_e / 2;

  int nt_x = ns_x * step0;   // range of indices   
  int nnbr_x = ns_x * (1 << level); // max # of neighboring points 

  for (int i = 0; i < n_neighbors; i++) {
    int index[n_dim]; 
    index[0] = essen_point->index[0] + off_x[i] * step_n; 

    if (index[0] < 0 || index[0] > nt_x) {
      essen_point->neighbors[i][0] = -1; // neighbor is out-of-range
    } else {
      essen_point->neighbors[i][0] = index[0]; 
      int flag; 
      coll_point_t *neighbor = add_coll_point(index, &flag,ld); 

      if (flag == 0) { // the point has just been created
        neighbor->parent[0] = essen_point->index[0]; 
        neighbor->coords[0] = ld->L_dim[0] * index[0] / nt_x; 
        neighbor->index[0] = index[0];
        neighbor->level = level + 1; 
        neighbor->stamp = stamp; 
        for (int j = 0; j < n_neighbors; j++) {
          for (int dir = 0; dir < n_dim; dir++) {
            neighbor->neighbors[j][dir] = -1; 
          }
        }

        // configure the wavelet stencil
        int indices_x[np] = {0}; 
        neighbor->wavelet_coef[0] = 
          get_stencil_type((index[0] - step_n) / step_e, nnbr_x - 1); 
        get_stencil_indices(index[0], nt_x, step_e, indices_x); 

        for (int j = 0; j < np; j++) {
          neighbor->wavelet_stencil[0][j][0] = indices_x[j]; 
          int flag1; 
          coll_point_t *temp = add_coll_point(&indices_x[j], &flag1,ld); 
          if (!flag1) { // the point has just been created
            create_nonessential_point(temp, &indices_x[j], stamp,ld); 
          } else if (temp->stamp < stamp) {
            // reason for setting status[1]: the point has a time stamp when it
            // is created. If the input stamp is t0, this branch never
            // happens. 
            temp->status[1] = nonessential; 
            advance_time_stamp(temp, stamp, 0,ld); 
          }
        }

        if (stamp == ld->t0) {
          neighbor->status[0] = neighboring; 
          initial_condition(neighbor->coords, neighbor->u[0],ld); 
          // in the initial grid construction phase, we need to check if this
          // neighboring point is an essential point by computing its wavelet
          // coefficient 
          double approx[n_variab + n_aux] = {0}; 
          forward_wavelet_trans(neighbor, 'u', mask,0, approx,ld); 
          for (int ivar = 0; ivar < n_variab; ivar++) {
            if (fabs(neighbor->u[0][ivar] - approx[ivar]) >= ld->eps[ivar]) {
              neighbor->status[0] = essential; 
              break;
            }
          }
          
          if (neighbor->status[0] == essential) {
            create_neighboring_point(neighbor, stamp,ld); 
          } else { // local refinement stops, update max_level
            ld->max_level = fmax(ld->max_level, neighbor->level);
          }
        } else {
          neighbor->status[1] = neighboring; 
          forward_wavelet_trans(neighbor, 'u', mask, 0, neighbor->u[0],ld);
          ld->max_level = fmax(ld->max_level, neighbor->level);
        }
      } else {        
        if (stamp == ld->t0) {
          // the point was previously created as an nonessential point in the
          // initial grid construction, we need to update its status to
          // neighboring and further checks if it is an essential point
          if (neighbor->status[0] == nonessential) {
            neighbor->status[0] = neighboring; 
            neighbor->parent[0] = essen_point->index[0]; 

            double approx[n_variab + n_aux] = {0}; 
            forward_wavelet_trans(neighbor, 'u', mask,0, approx,ld); 
            for (int ivar = 0; ivar < n_variab; ivar++) {
              if (fabs(neighbor->u[0][ivar] - approx[ivar]) >= ld->eps[ivar]) {
                neighbor->status[0] = essential; 
                break;
              }
            }

            if (neighbor->status[0] == essential) {
              create_neighboring_point(neighbor, stamp,ld);
            } else {
              ld->max_level = fmax(ld->max_level, neighbor->level); 
            }
          }
        } else {
          // this branch will be executed in the following situation: when
          // reaching a new time stamp, all previously active points perform
          // wavelet transform to determine the new status. some neighboring
          // point becomes essential and needs to add neighboring points around
          // itself. the point to be added already exists in the grid as an
          // nonessential point

          // 1. update the tree structure because the nonessential point's
          // parent field is not set
          neighbor->parent[0] = essen_point->index[0]; 

          // 2. update the status of the point
          neighbor->status[1] = neighboring; 

          // 3. check the time stamp of the collocation point in its wavelet
          // stencil. if the time stamp is not up-to-date, we need to advance
          // those points
          for (int dir = 0; dir < n_dim; dir++) {
            if (neighbor->wavelet_coef[dir] != -1) {
              for (int j = 0; j < np; j++) {
                coll_point_t *temp = 
                  get_coll_point(neighbor->wavelet_stencil[dir][j],ld); 
                if (temp->stamp < stamp) {
                  temp->status[1] = nonessential; 
                  advance_time_stamp(temp, stamp, 0,ld);
                } 
              }
            }
          }

          // 4. perform wavelet transform to set value of the neighboring point 
          forward_wavelet_trans(neighbor, 'u', mask, 0, neighbor->u[0],ld); 

          // 5. update time stamp
          neighbor->stamp = stamp; 
        }
      }
    }  // move on to the next neighboring point 
  } // i 
}

/// ----------------------------------------------------------------------------
/// @brief Add a collocation point to the data store. This point will be stored 
/// in the hash table part. 
/// @note When distributed, this function should be protected by lock.
/// ----------------------------------------------------------------------------
coll_point_t *add_coll_point(const int index[n_dim], int *flag,Domain *ld) {
  // check if the point exists in the hash table
  coll_point_t *retval = get_coll_point(index,ld);

  if (retval != NULL) {
    *flag = 1; // the point already exists
  } else {
    *flag = 0; // the point does not exist
    uint64_t mkey = morton_key(index);
    uint64_t hidx = hash(mkey);
    hash_entry_t *h_entry = calloc(1, sizeof(hash_entry_t));
    retval = calloc(1, sizeof(coll_point_t));
    assert(h_entry != NULL);
    assert(retval != NULL);
    h_entry->point = retval;
    h_entry->mkey = mkey;
    h_entry->next = ld->coll_points->hash_table[hidx];
    ld->coll_points->hash_table[hidx] = h_entry;
  }

  return retval;
}

/// ----------------------------------------------------------------------------
/// @brief Add nonessential point
/// ----------------------------------------------------------------------------
void create_nonessential_point(coll_point_t *nonessen_point,
                               const int index[n_dim], const double stamp,Domain *ld) {
  int mask[n_variab + n_aux] = {0};
  for (int ivar = 0; ivar < n_variab; ivar++)
    mask[ivar] = 1;

  // determine the level to which this nonessential point belongs 
  int level = 0;
  for (int dir = 0; dir < n_dim; dir++) {
    int ilev = get_level(index[dir]);
    level = (ilev >= level ? ilev : level);
  }

  // assert that the level of the nonessential point is more than 1
  assert(level > 1);

  // spacing between grid points at the nonessential point level 
  int step_n = 1 << (JJ - level);

  // spacing between grid points of the stencils for the nonessential point
  int step2 = step_n * 2;

  int indices_x[np] = {0};
  int nt_x = ns_x * (1 << JJ); // range of indices at level JJ

  // max # of nonessential points can be created at the specified level
  int nonessen_x = ns_x * (1 << (level - 1));

  nonessen_point->coords[0] = ld->L_dim[0] * index[0] / nt_x;
  nonessen_point->index[0] = index[0];
  nonessen_point->level = level;
  nonessen_point->stamp = stamp;
  for (int j = 0; j < n_neighbors; j++) {
    for (int dir = 0; dir < n_dim; dir++) {
      nonessen_point->neighbors[j][dir] = -1;
    }
  }

  nonessen_point->wavelet_coef[0] =
    get_stencil_type((index[0] - step_n) / step2, nonessen_x - 1);
  get_stencil_indices(index[0], nt_x, step2, indices_x);

  for (int i = 0; i < np; i++) {
    int flag;
    nonessen_point->wavelet_stencil[0][i][0] = indices_x[i];
    coll_point_t *temp = add_coll_point(&indices_x[i], &flag,ld);
    if (!flag) {
      create_nonessential_point(temp, &indices_x[i], stamp,ld);
    } else if (temp->stamp < stamp) {
       // reason for setting status[1]: a point created in the initial grid has
      // time stamp t0. if the input stamp is t0, this branch is never going to
      // be executed. 
      temp->status[1] = nonessential;
      advance_time_stamp(temp, stamp, 0,ld);
    }
  }

  if (stamp == ld->t0) {
    initial_condition(nonessen_point->coords, nonessen_point->u[0],ld);
  } else {
    forward_wavelet_trans(nonessen_point, 'u', mask, 0, nonessen_point->u[0],ld);
  }

  // if the input stamp is t0, we should set status[0]. if the input stamp is
  // not t0, we should set status[1]. however, a nonessential point does not
  // participate time marching. as a result, if the point is created during the
  // execution of adjust_grids() function, if we set status[1] here, we need to
  // eventually overwrite status[0] with status[1] and set status[1] to
  // unitialized, and for this reason, we just directly set status[0] here. 
  nonessen_point->status[0] = nonessential;
}

/// ----------------------------------------------------------------------------
/// @brief Advance time stamp of the specified collocation point
/// ----------------------------------------------------------------------------
void advance_time_stamp(coll_point_t *point, const double stamp,
                        const int gen,Domain *ld) {
  // make sure the time stamps of the stencil are up-to-date
  for (int dir = 0; dir < n_dim; dir++) {
    if (point->wavelet_coef[dir] != -1) {
      for (int j = 0; j < np; j++) {
        coll_point_t *temp =
          get_coll_point(point->wavelet_stencil[dir][j],ld);
        if (temp->stamp < stamp) {
          temp->status[1] = nonessential;
          advance_time_stamp(temp, stamp, gen,ld);
        }
      }
    }
  }

  // perform wavelet transform to set value of the point
  int mask[n_variab + n_aux] = {0};
  for (int ivar = 0; ivar < n_variab; ivar++)
    mask[ivar] = 1;

  forward_wavelet_trans(point, 'u', mask, 0, point->u[gen],ld);

  // update the time stamp
  point->stamp = stamp;
}

/// @brief Determines the level to which the index under consideration belongs
/// ---------------------------------------------------------------------------
int get_level(const int index) {
  int level = 0;

  for (int i = JJ; i >= 0; i--) {
    int step = 1 << i;
    if (index % step == 0) {
      level = JJ - i;
      break;
    }
  }

  return level;
}

/// @brief Set up derivative stencils for all active points
/// ---------------------------------------------------------------------------
void deriv_stencil_config(const double stamp,Domain *ld) {
  int type = (stamp == ld->t0 ? 0 : 1);
  for (int i = 0; i <= ns_x * 2; i++) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    get_deriv_stencil(point, 0, stamp,ld); // point, direction, stamp
    if (point->level == 1 && point->status[type] == essential)
      deriv_stencil_helper(point, 0, stamp,ld);
  }
}

/// ---------------------------------------------------------------------------
/// @brief Helper function to recurisvely traverse all the active points to 
/// determine the derivative stencil
/// ---------------------------------------------------------------------------
void deriv_stencil_helper(coll_point_t *point, const int dir,
                          const double stamp,Domain *ld) {
  int type = (stamp == ld->t0 ? 0 : 1);
  for (int i = 0; i < n_neighbors; i++) {
    if (point->neighbors[i][0] != -1) {
      coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
      get_deriv_stencil(neighbor, dir, stamp,ld);
      if (neighbor->status[type] == essential)
        deriv_stencil_helper(neighbor, dir, stamp,ld);
    }
  }
}

/// @brief Find derivative stencil in the specified direction. For
/// each point found in the stencil, we need to check its status and
/// if the status is nonessential, its kill flag needs to be set to false.
/// ---------------------------------------------------------------------------
void get_deriv_stencil(coll_point_t *point, const int dir, const double stamp,Domain *ld) {
  int upper_limit[n_dim];
  upper_limit[0] = ns_x * (1 << JJ);

  int index[n_dim];
  for (int i = 0; i < n_dim; i++)
    index[i] = point->index[i];

  int type = (stamp == ld->t0 ? 0 : 1);
  coll_point_t *closest_point = get_closest_point(point, dir, type,ld);
  int step = fabs(point->index[dir] - closest_point->index[dir]);
  int mid = (nstn - 1) / 2; // nstn is an odd number 

  if (point->index[dir] - mid * step < 0) {
    for (int i = 0; i < nstn; i++) {
      index[dir] = i * step;
      if (i * step == point->index[dir])
        point->derivative_coef[dir] = i;
      point->derivative_stencil[dir][i][0] = index[0];
      int flag;
      coll_point_t *temp = add_coll_point(index, &flag,ld);
      if (!flag) {
        create_nonessential_point(temp, index, stamp,ld);
      } else if (temp->stamp < stamp) {
        // reason for setting status[1]: each point has the time stamp of its
        // creation. If the input stamp is t0, this branch is never going to be
        // executed. the point's new status should be nonessential because its
        // time stamp is outdated.  
        temp->status[1] = nonessential;
        advance_time_stamp(temp, stamp, 0,ld);
      }
    }
  } else if (point->index[dir] + mid * step > upper_limit[dir]) {
    for (int i = nstn - 1; i >= 0; i--) {
      index[dir] = upper_limit[dir] - i * step;
      if (index[dir] == point->index[dir])
        point->derivative_coef[dir] = nstn - 1 - i;
      point->derivative_stencil[dir][i][0] = index[0];
      int flag;
      coll_point_t *temp = add_coll_point(index, &flag,ld);
      if (!flag) {
        create_nonessential_point(temp, index, stamp,ld);
      } else if (temp->stamp < stamp) {
        temp->status[1] = nonessential;
        advance_time_stamp(temp, stamp, 0,ld);
      }
    }
  } else {
    point->derivative_coef[dir] = mid;
    point->derivative_stencil[dir][mid][0] = point->index[0];

    int flag;
    coll_point_t *temp;
    for (int i = 1; i <= mid; i++) {
      index[dir] = point->index[dir] + i * step;
      point->derivative_stencil[dir][mid + i][0] = index[0];
      temp = add_coll_point(index, &flag,ld);
      if (!flag) {
        create_nonessential_point(temp, index, stamp,ld);
      } else if (temp->stamp < stamp) {
        temp->status[1] = nonessential;
        advance_time_stamp(temp, stamp, 0,ld);
      }

      index[dir] = point->index[dir] - i * step;
      point->derivative_stencil[dir][mid - i][0] = index[0];
      temp = add_coll_point(index, &flag,ld);
      if (!flag) {
        create_nonessential_point(temp, index, stamp,ld);
      } else if (temp->stamp < stamp) {
        temp->status[1] = nonessential;
        advance_time_stamp(temp, stamp, 0,ld);
      }
    }
  }
}

/// ---------------------------------------------------------------------------
/// @brief Find the closest active point in the specified direction
/// ---------------------------------------------------------------------------
coll_point_t *get_closest_point(coll_point_t *point, const int dir,
                                const int type,Domain *ld) {
  coll_point_t *retval = NULL;
  int step = 1 << (JJ - point->level);

  if (point->status[type] == neighboring) {
    // for an neighboring point, the closet point is either its parent or among
    // the neighbors of its parent 
    int indx1 = point->index[dir] + step;
    int indx2 = point->index[dir] - step;
    coll_point_t *parent = get_coll_point(point->parent,ld);

    if (parent->index[dir] == indx1 || parent->index[dir] == indx2) {
      retval = parent;
    } else {
      for (int i = 0; i < n_neighbors; i++) {
        coll_point_t *neighbor = get_coll_point(parent->neighbors[i],ld);
        if (neighbor->index[dir] == indx1 || neighbor->index[dir] == indx2) {
          retval = neighbor;
          break;
        }
      }
    }
  } else {
    // for an essential point, search the +/- of the specified direction
    int indx1 = diag[2 * dir];
    int indx2 = diag[2 * dir + 1];
    int dist1 = INT_MAX, dist2 = INT_MAX;
    coll_point_t *tmp1, *tmp2;

    // the first step is the + direction, then all the following search is along
    // the - direction
    if (point->neighbors[indx1][0] != -1) {
      tmp1 = get_coll_point(point->neighbors[indx1],ld);
      while (tmp1->status[type] == essential &&
             tmp1->neighbors[indx2][0] != -1)
        tmp1 = get_coll_point(tmp1->neighbors[indx2],ld);

      dist1 = fabs(point->index[dir] - tmp1->index[dir]);
    }

    // search - direction first, followed by the + direction
    if (point->neighbors[indx2][0] != -1) {
      tmp2 = get_coll_point(point->neighbors[indx2],ld);
      while (tmp2->status[type] == essential &&
             tmp2->neighbors[indx1][0] != -1)
        tmp2 = get_coll_point(tmp2->neighbors[indx1],ld);

      dist2 = fabs(point->index[dir] - tmp2->index[dir]);
    }

    retval = (dist1 <= dist2 ? tmp1 : tmp2);
  }

  return retval;
}

/// ---------------------------------------------------------------------------
/// @brief Compute the global time increment
/// ---------------------------------------------------------------------------
double get_global_dt(Domain *ld) {
  double global_dt = DBL_MAX;
  for (int i = 0; i <= ns_x * 2; i++) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    double local_dt = get_local_dt(point,ld);
    global_dt = fmin(local_dt, global_dt);
    if (point->level == 1 && point->status[0] == essential)
      global_dt_helper(point, &global_dt,ld);
  }
  return global_dt;
}

/// ---------------------------------------------------------------------------
/// @brief Helper function to recursively traverse all the active points to 
/// perform global reduction on the time increment
/// ---------------------------------------------------------------------------
void global_dt_helper(coll_point_t *point, double *dt,Domain *ld) {
  for (int i = 0; i < n_neighbors; i++) {
    if (point->neighbors[i][0] != -1) {
      coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
      double local_dt = get_local_dt(neighbor,ld);
      *dt = fmin(*dt, local_dt);
      if (neighbor->status[0] == essential)
        global_dt_helper(neighbor, dt,ld);
    }
  }
}

double get_local_dt(const coll_point_t *point,Domain *ld) {
  // dx is the grid spacing between the collocation point itself and the closet
  // point used in its derivative stencil
  double dx0 = ld->L_dim[0] / ns_x / (1 << JJ);
  double dx = dx0 * fabs(point->derivative_stencil[0][0][0] -
                         point->derivative_stencil[0][1][0]);
  double rho = point->u[0][0];
  double rho_u = point->u[0][1];
  double rho_E = point->u[0][2];
  double u = rho_u / rho;
  double E = rho_E / rho;
  double p = (ld->Gamma_r - 1) * rho * (E - 0.5 * u * u);
  double dt1 = u / dx;
  double dt2 = 2 * ld->mur / rho / dx / dx;
  double dt3 = sqrt(ld->Gamma_r * p / rho) / dx;
  double dt = 0.6 / (dt1 + dt2 + dt3);
  return dt;
}

void apply_time_integrator(const double t, const double dt,Domain *ld) {
  // compute k1 = f(tn, u[0][*])
  compute_rhs(t, 0,ld);

  // compute u[1][*] = u[0][*] + 0.5 * dt * k1
  for (int i = 0; i <= ns_x * 2; i++)
    rk4_helper1(&(ld->coll_points->array[i]), dt);

  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      integrator_helper(point, dt, rk4_helper1,ld);
  }

  // get u[1][*] for the nonessential points involved
  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper2(point, 1,ld);
  }

  // compute k2 = f(tn + 0.5 * dt, u[1][*]) 
  compute_rhs(t + 0.5 * dt, 1,ld);

  // compute u[2][*] = u[0][*] + 0.5 * dt * k2
  for (int i = 0; i <= ns_x * 2; i++)
    rk4_helper2(&(ld->coll_points->array[i]), dt);

  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      integrator_helper(point, dt, rk4_helper2,ld);
  }

  // get u[2][*] for the nonessential points involved
  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper2(point, 2,ld);
  }

  // compute k3 = f(tn + 0.5 * dt, u[2][*])
  compute_rhs(t + 0.5 * dt, 2,ld);

  // compute u[3][*] = u[0][*] + dt * k3
  for (int i = 0; i <= ns_x * 2; i++)
    rk4_helper3(&(ld->coll_points->array[i]), dt);

  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      integrator_helper(point, dt, rk4_helper3,ld);
  }

  // get u[3][*] for the nonessential points involved
  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper2(point, 3,ld);
  }

  // compute k4 = f(tn + dt, u[3][*]) 
  compute_rhs(t + dt, 3,ld);

  // update u[0][*]
  for (int i = 0; i <= ns_x * 2; i++)
    rk4_helper4(&(ld->coll_points->array[i]), dt);

  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      integrator_helper(point, dt, rk4_helper4,ld);
  }

  // update u[0][*] for the nonessential points involved
  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper2(point, 0,ld);
  }

  for (int i = 1; i < ns_x * 2; i += 2) {
    coll_point_t *point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      stage_reset_helper(point,ld);
  }
}

void rk4_helper1(coll_point_t *point, const double dt) {
  double *u_curr = point->u[0];
  double *u_next = point->u[1];
  double *rhs = point->rhs[0];

  for (int i = 0; i < n_variab; i++)
    u_next[i] = u_curr[i] + 0.5 * dt * rhs[i];

  point->stamp += dt / n_gen;
}

void rk4_helper2(coll_point_t *point, const double dt) {
  double *u_curr = point->u[0];
  double *u_next = point->u[2];
  double *rhs = point->rhs[1];

  for (int i = 0; i < n_variab; i++)
    u_next[i] = u_curr[i] + 0.5 * dt * rhs[i];

  point->stamp += dt / n_gen;
}

void rk4_helper3(coll_point_t *point, const double dt) {
  double *u_curr = point->u[0];
  double *u_next = point->u[3];
  double *rhs = point->rhs[2];

  for (int i = 0; i < n_variab; i++)
    u_next[i] = u_curr[i] + dt * rhs[i];

  point->stamp += dt / n_gen;
}

void rk4_helper4(coll_point_t *point, const double dt) {
  double *u_curr = point->u[0];
  double *k1 = point->rhs[0];
  double *k2 = point->rhs[1];
  double *k3 = point->rhs[2];
  double *k4 = point->rhs[3];

  for (int i = 0; i < n_variab; i++)
    u_curr[i] += dt * (k1[i] + k2[i] * 2 + k3[i] * 2 + k4[i]) / 6;

  point->stamp += dt / n_gen;
}

void compute_rhs(const double t, const int gen,Domain *ld) {
  coll_point_t *point;

  // Stage 1: compute u, E, e, p, T, and d(rho*u)/dx
  for (int i = 0; i <= ns_x * 2; i++) {
    point = &(ld->coll_points->array[i]);
    rhs_active_stage1(point, gen,ld);
  }

  for (int i = 1; i < ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      rhs_helper(point, gen, rhs_active_stage1,ld);
  }

  for (int i = 0; i <= ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper1(point, 1, gen, rhs_nonessen_stage1,ld);
  }

  // Stage 2: compute dT/dx, dp/dx, du/dx, set q and tau, apply boundary
  // condition and set rho*u*u - tau and (rho*E + p)*u + q - u*tau
  for (int i = 0; i <= ns_x * 2; i++) {
    point = &(ld->coll_points->array[i]);
    rhs_active_stage2(point, gen,ld);
  }

  for (int i = 1; i < ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      rhs_helper(point, gen, rhs_active_stage2,ld);
  }

  for (int i = 0; i <= ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper1(point, 2, gen, rhs_nonessen_stage2,ld);
  }

  // Stage 3: compute d/(rho*u*u - tau)/dx and d[(rho*E + p)*u + q - u*tau]/dx,
  // set rhs[0], rhs[1], and rhs[2]
  for (int i = 0; i <= ns_x * 2; i++) {
    point = &(ld->coll_points->array[i]);
    rhs_active_stage3(point, gen,ld);
  }

  for (int i = 1; i < ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      rhs_helper(point, gen, rhs_active_stage3,ld);
  }


  for (int i = 0; i <= ns_x * 2; i += 2) {
    point = &(ld->coll_points->array[i]);
    if (point->status[0] == essential)
      deriv_nonessen_helper1(point, 3, gen, rhs_nonessen_stage3,ld);
  }
}

void rhs_active_stage1(coll_point_t *point, const int gen,Domain *ld) {
  double rho = point->u[gen][0];
  double rho_u = point->u[gen][1];
  double rho_E = point->u[gen][2];

  double u = rho_u / rho;
  double E = rho_E / rho;
  double e = E - 0.5 * u * u;
  double p = (ld->Gamma_r - 1) * rho * e;
  double T = e / ld->c_v;

  point->u[gen][3] = T;
  point->u[gen][4] = p;
  point->u[gen][5] = u;
  point->du[gen][0] = get_deriv(1, 0, gen, 1, point,ld); // d(rho*u)/dx
}

void rhs_nonessen_stage1(coll_point_t *point, const int gen,Domain *ld) {
  // compute u[0], u[1], u[2] via wavelet transform if gen >= 1
  if (gen) {
    int mask[n_variab + n_aux] = {0};
    for (int ivar = 0; ivar < n_variab; ivar++)
      mask[ivar] = 1;
    double approx[n_variab + n_aux] = {0};
    forward_wavelet_trans(point, 'u', mask, gen, approx,ld);
    point->u[gen][0] = approx[0];
    point->u[gen][1] = approx[1];
    point->u[gen][2] = approx[2];
  }

  double rho = point->u[gen][0];
  double rho_u = point->u[gen][1];
  double rho_E = point->u[gen][2];

  double u = rho_u / rho;
  double E = rho_E / rho;
  double e = E - 0.5 * u * u;
  double p = (ld->Gamma_r - 1) * rho * e;
  double T = e / ld->c_v;

  point->u[gen][3] = T;
  point->u[gen][4] = p;
  point->u[gen][5] = u;

  int mask[n_deriv] = {1, 0, 0, 0, 0, 0};
  double approx[n_deriv] = {0};
  forward_wavelet_trans(point, 'd', mask, gen, approx,ld);
  point->du[gen][0] = approx[0];
}

void rhs_active_stage2(coll_point_t *point, const int gen,Domain *ld) {
  static const int upper_bound = ns_x * (1 << JJ);

  point->du[gen][1] = get_deriv(3, 0, gen, 1, point,ld); // dT/dx
  point->du[gen][2] = get_deriv(4, 0, gen, 1, point,ld); // dp/dx
  point->du[gen][3] = get_deriv(5, 0, gen, 1, point,ld); // du/dx

  double tau = 4 / 3 * ld->mur * point->du[gen][3];
  double q = -ld->k_r * point->du[gen][1];

  double rho_u = point->u[gen][1];
  double rho_E = point->u[gen][2];
  double p = point->u[gen][4];
  double u = point->u[gen][5];

  if (point->index[0] == 0 || point->index[0] == upper_bound) {
    tau = 0;
    q = 0;
  }

  point->u[gen][6] = rho_u * u - tau;
  point->u[gen][7] = (rho_E + p) * u + q - u * tau;
}

void rhs_nonessen_stage2(coll_point_t *point, const int gen,Domain *ld) {
  // compute dT/dx, dp/dx, du/dx from wavelet transform
  int mask[n_deriv] = {0, 1, 1, 1, 0, 0};
  double approx[n_deriv] = {0};
  forward_wavelet_trans(point, 'd', mask, gen, approx,ld);
  point->du[gen][1] = approx[1]; // dT/dx
  point->du[gen][2] = approx[2]; // dp/dx
  point->du[gen][3] = approx[3]; // du/dx

  double tau = 4 / 3 * ld->mur * point->du[gen][3];
  double q = -ld->k_r * point->du[gen][1];

  double rho_u = point->u[gen][1];
  double rho_E = point->u[gen][2];
  double p = point->u[gen][4];
  double u = point->u[gen][5];

  point->u[gen][6] = rho_u * u - tau;
  point->u[gen][7] = (rho_E + p) * u + q - u *tau;
}

void rhs_active_stage3(coll_point_t *point, const int gen,Domain *ld) {
  point->du[gen][4] = get_deriv(6, 0, gen, 1, point,ld);
  point->du[gen][5] = get_deriv(7, 0, gen, 1, point,ld);

  point->rhs[gen][0] = -point->du[gen][0];
  point->rhs[gen][1] = -point->du[gen][4] - point->du[gen][2];
  point->rhs[gen][2] = -point->du[gen][5];
}

void rhs_nonessen_stage3(coll_point_t *point, const int gen,Domain *ld) {
  int mask[n_deriv] = {0, 0, 0, 0, 1, 1};
  double approx[n_deriv] = {0};
  forward_wavelet_trans(point, 'd', mask, gen, approx,ld);
  point->du[gen][4] = approx[4];
  point->du[gen][5] = approx[5];

  // set rhs[0], rhs[1], rhs[2]
  point->rhs[gen][0] = -point->du[gen][0];
  point->rhs[gen][1] = -point->du[gen][4] - point->du[gen][2];
  point->rhs[gen][2] = -point->du[gen][5];
}

/// @brief Helper function to recursively traverse all the active points to 
/// perform time integration
/// ---------------------------------------------------------------------------
void integrator_helper(coll_point_t *point, const double dt,
                       void(*func)(coll_point_t *, const double),Domain *ld) {
  for (int i = 0; i < n_neighbors; i++) {
    if (point->neighbors[i][0] != -1) {
      coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
      func(neighbor, dt);
      if (neighbor->status[0] == essential)
        integrator_helper(neighbor, dt, func,ld);
    }
  }
}

/// ---------------------------------------------------------------------------
/// @brief Helper function to recursively traverse all the active points to 
/// compute the right hand side function
/// ---------------------------------------------------------------------------
void rhs_helper(coll_point_t *point, const int gen,
                void(*func)(coll_point_t *, const int,Domain *ld),Domain *ld) {
  for (int i = 0; i < n_neighbors; i++) {
    if (point->neighbors[i][0] != -1) {
      coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
      func(neighbor, gen,ld);
      if (neighbor->status[0] == essential)
        rhs_helper(neighbor, gen, func,ld);
    }
  }
}

/// ---------------------------------------------------------------------------
/// @brief Helper function to traverse all the nonessential points used in the
/// derivative computation and advance their time stamp. This helper function is
/// intended to be used before starting a new generation of the time integrator
/// (such as before computing k2 of the RK4)
/// ---------------------------------------------------------------------------
void deriv_nonessen_helper2(coll_point_t *point, const int gen,Domain *ld) {
  for (int dir = 0; dir < n_dim; dir++) {
    for (int i = 0; i < nstn; i++) {
      coll_point_t *temp = get_coll_point(point->derivative_stencil[dir][i],ld);
      if (temp->status[0] == nonessential && temp->stamp < point->stamp)
        advance_time_stamp(temp, point->stamp, gen,ld);
    }
  }

  if (point->status[0] == essential) {
    for (int i = 0; i < n_neighbors; i++) {
      if (point->neighbors[i][0] != -1) {
        coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
        deriv_nonessen_helper2(neighbor, gen,ld);
      }
    }
  }
}

/// @brief Reset stage
/// ----------------------------------------------------------------------------
void stage_reset_helper(coll_point_t *point,Domain *ld) {
  for (int i = 0; i < n_neighbors; i++) {
    if (point->neighbors[i][0] != -1) {
      coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
      for (int dir = 0; dir < n_dim; dir++) {
        if (neighbor->wavelet_coef[dir] != -1) {
          for (int j = 0; j < np; j++) {
            coll_point_t *temp =
              get_coll_point(neighbor->wavelet_stencil[dir][j],ld);
            if (temp->status[0] == nonessential && temp->stage[0] != -1) {
              for (int igen = 0; igen < n_gen; igen++)
                temp->stage[igen] = -1;
            }
          }
        }

        for (int j = 0; j < nstn; j++) {
          coll_point_t *temp =
            get_coll_point(neighbor->derivative_stencil[dir][j],ld);
          if (temp->status[0] == nonessential && temp->stage[0] != -1) {
            for (int igen = 0; igen < n_gen; igen++)
              temp->stage[igen] = -1;
          }
        }
      }

      if (neighbor->status[0] == essential)
        stage_reset_helper(neighbor,ld);
    }
  }
}

/// ---------------------------------------------------------------------------
/// @brief Helper function to traverse all the nonessential points used in the 
/// derivative computation and apply function func. this helper function is
/// intended to be used within the right hand side computation of a particular
/// stage of the specified generation
/// ---------------------------------------------------------------------------
void deriv_nonessen_helper1(coll_point_t *point, const int stage, const int gen,
                            void(*func)(coll_point_t *, const int,Domain *ld),Domain *ld) {
  for (int dir = 0; dir < n_dim; dir++) {
    for (int i = 0; i < nstn; i++) {
      coll_point_t *temp = get_coll_point(point->derivative_stencil[dir][i],ld);
      if (temp->status[0] == nonessential && temp->stage[gen] < stage) {
        func(temp, gen,ld);
        temp->stage[gen] = stage;
      }
    }
  }

  if (point->status[0] == essential) {
    for (int i = 0; i < n_neighbors; i++) {
      if (point->neighbors[i][0] != -1) {
        coll_point_t *neighbor = get_coll_point(point->neighbors[i],ld);
        deriv_nonessen_helper1(neighbor, stage, gen, func,ld);
      }
    }
  }
}

/// @brief Compute derivative of a variable in the specified direction
/// ---------------------------------------------------------------------------
double get_deriv(const int ivar, const int dir, const int gen,
                 const int order, coll_point_t *point,Domain *ld) {
  double dvar = 0; // variable for derivative
  int derivative_coef = point->derivative_coef[dir];
  double *coef = (order == 1 ?
                  ld->nfd_diff_coeff[0][derivative_coef] :
                  ld->nfd_diff_coeff[1][derivative_coef]);

  coll_point_t *derivative_stencil[nstn] = {NULL};
  for (int i = 0; i < nstn; i++)
    derivative_stencil[i] = get_coll_point(point->derivative_stencil[dir][i],ld);

  double dx = derivative_stencil[1]->coords[dir] -
    derivative_stencil[0]->coords[dir];
  double scale = pow(fabs(dx), -order);

  for (int i = 0; i < nstn; i++)
    dvar += coef[i] * derivative_stencil[i]->u[gen][ivar];

  dvar *= scale;
  return dvar;
}




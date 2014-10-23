
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

/// @brief Retrieve collocation point based on its Morton key  
/// ---------------------------------------------------------------------------
coll_point_t *get_coll_point(const int index[n_dim],hpx_addr_t basecollpoints,hpx_addr_t collpoints) {
  static const int step = 1 << (JJ - 1);
  coll_point_t *retval = NULL;

  bool stored_in_array = true;
  for (int dir = 0; dir < n_dim; dir++)
    stored_in_array &= (index[dir] % step == 0);

  if (stored_in_array) {
    hpx_addr_t there = hpx_addr_add(basecollpoints,(index[0] / step)*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_addr_t complete = hpx_lco_and_new(1);
    coll_point_t *point = malloc(sizeof(*point));
    hpx_gas_memget(point,there,sizeof(coll_point_t),complete);
    hpx_lco_wait(complete);
    hpx_lco_delete(complete,HPX_NULL);
    return point;
  } else {
    uint64_t mkey = morton_key(index);
    uint64_t hidx = hash(mkey);
    printf(" PUT BIG NUMBER TEST HERE %ld\n",hidx);
    hash_entry_t *curr = malloc(sizeof(*curr)); 
    hpx_addr_t there = hpx_addr_add(collpoints,hidx*sizeof(hash_entry_t),sizeof(hash_entry_t));
    hpx_addr_t complete = hpx_lco_and_new(1);
    hpx_gas_memget(curr,there,sizeof(hash_entry_t),complete);
    hpx_lco_wait(complete);
    hpx_lco_delete(complete,HPX_NULL);
    while (curr->initialized) {
      if (curr->mkey == mkey) {
        retval = &curr->point;
        break;
      }
      hpx_addr_t finished = hpx_lco_and_new(1);
      hpx_addr_t next = curr->next;
      hpx_gas_memget(curr,next,sizeof(hash_entry_t),finished);
      hpx_lco_wait(finished);
      hpx_lco_delete(finished,HPX_NULL);
    }
  }

  return retval;
}

int _cag_action(Cag_action_helper *ld)
{
  hpx_addr_t local = hpx_thread_current_target();
  coll_point_t *point = NULL;
  if (!hpx_gas_try_pin(local, (void**)&point))
    return HPX_RESEND;

  if (point->status[0] == essential)
      create_neighboring_point(point, ld->t0,ld->basecollpoints,ld->collpoints);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

/// @brief Create adaptive grid 
/// ----------------------------------------------------------------------------
void create_adap_grids(Domain *ld) {

  Cag_action_helper *cag;
  cag = malloc((ns_x*2+1)/2*sizeof(Cag_action_helper));
  hpx_addr_t complete = hpx_lco_and_new((ns_x*2+1)/2);
  for (int i = 1; i <= ns_x * 2; i += 2) {
    cag[i].i = i;
    cag[i].t0 = ld->t0;
    cag[i].basecollpoints = ld->basecollpoints;
    cag[i].collpoints = ld->collpoints;
    hpx_addr_t there = hpx_addr_add(ld->basecollpoints,i*sizeof(coll_point_t),sizeof(coll_point_t));
    hpx_call(there,_cag,&cag[i],sizeof(Cag_action_helper),complete);
  }
  hpx_lco_wait(complete);
  hpx_lco_delete(complete,HPX_NULL);
  free(cag);

}
/// ----------------------------------------------------------------------------
/// @brief Add neighboring point around an essential point
/// ----------------------------------------------------------------------------
void create_neighboring_point(coll_point_t *essen_point, const double stamp,hpx_addr_t basecollpoints,hpx_addr_t collpoints) {
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

      coll_point_t *neighbor = add_coll_point(index, &flag,basecollpoints,collpoints); 
#if 0

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
#endif
    }  // move on to the next neighboring point 
  } // i 
}

/// ----------------------------------------------------------------------------
/// @brief Add a collocation point to the data store. This point will be stored 
/// in the hash table part. 
/// @note When distributed, this function should be protected by lock.
/// ----------------------------------------------------------------------------
coll_point_t *add_coll_point(const int index[n_dim], int *flag,hpx_addr_t basecollpoints,hpx_addr_t collpoints) {
  // check if the point exists in the hash table

  coll_point_t *retval = get_coll_point(index,basecollpoints,collpoints);
#if 0

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
#endif

  return retval;
}
#if 0

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
#endif

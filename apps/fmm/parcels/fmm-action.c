/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Implementations of FMM actions
/// ----------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include "fmm.h"

hpx_addr_t sources;
hpx_addr_t charges;
hpx_addr_t targets;
hpx_addr_t potential;
hpx_addr_t field;
hpx_addr_t mapsrc;
hpx_addr_t maptar;
hpx_addr_t source_root;
hpx_addr_t target_root;

const int xoff[] = {0, 1, 0, 1, 0, 1, 0, 1};
const int yoff[] = {0, 0, 1, 1, 0, 0, 1, 1};
const int zoff[] = {0, 0, 0, 0, 1, 1, 1, 1};

int _fmm_main_action(void) {

  // Allocate memory to hold input and output data, and mapping info. 
  sources = hpx_gas_alloc(nsources * 3, sizeof(double)); 
  charges = hpx_gas_alloc(nsources, sizeof(double));
  targets = hpx_gas_alloc(ntargets * 3, sizeof(double));
  potential = hpx_gas_alloc(ntargets, sizeof(double));
  field = hpx_gas_alloc(ntargets * 3, sizeof(double));
  mapsrc = hpx_gas_alloc(nsources, sizeof(int)); 
  maptar = hpx_gas_alloc(ntargets, sizeof(int)); 

  // Populate test data and initialize mapping
  hpx_addr_t bound_src = hpx_lco_future_new(sizeof(double) * 6); 
  hpx_addr_t bound_tar = hpx_lco_future_new(sizeof(double) * 6); 
  hpx_addr_t init_data_done = hpx_lco_and_new(3); 

  hpx_call(sources, _init_sources, NULL, 0, bound_src); 
  hpx_call(charges, _init_charges, NULL, 0, bound_tar);
  hpx_call(targets, _init_targets, NULL, 0, init_data_done); 
  hpx_call(mapsrc, _init_mapsrc, NULL, 0, init_data_done);
  hpx_call(maptar, _init_maptar, NULL, 0, init_data_done); 
  hpx_lco_wait(init_data_done); 
  hpx_lco_delete(init_data_done, HPX_NULL); 
  
  // Determine the smallest box enclosing all the points
  double temp_src[6] = {0}, temp_tar[6] = {0}; 
  hpx_lco_get(bound_src, sizeof(double) * 6, temp_src); 
  hpx_lco_get(bound_tar, sizeof(double) * 6, temp_tar); 
  hpx_lco_delete(bound_src, HPX_NULL); 
  hpx_lco_delete(bound_tar, HPX_NULL); 

  double xmin = fmin(temp_src[0], temp_tar[0]); 
  double xmax = fmax(temp_src[1], temp_tar[1]); 
  double ymin = fmin(temp_src[2], temp_tar[2]); 
  double ymax = fmax(temp_src[3], temp_tar[3]); 
  double zmin = fmin(temp_src[4], temp_tar[4]); 
  double zmax = fmax(temp_src[5], temp_tar[5]); 
  double size = fmax(fmax(xmax - xmin, ymax - ymin), zmax - zmin);

  // Construct root nodes of the source and target tree
  hpx_addr_t init_root_done = hpx_lco_and_new(2); 
  source_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); // expansion is NULL
  target_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); // expansion is NULL 
  hpx_call(source_root, _init_source_root, NULL, 0, init_root_done); 
  hpx_call(target_root, _init_target_root, &source_root, 
	   sizeof(hpx_addr_t), init_root_done); 
  hpx_lco_wait(init_root_done); 
  hpx_lco_delete(init_root_done, HPX_NULL); 

  // Construct FMM param on each locality
  hpx_addr_t init_param_done = hpx_lco_future_new(0); 
  init_param_action_arg_t init_param_arg = {
    .sources = sources, 
    .charges = charges, 
    .targets = targets, 
    .potential = potential, 
    .field = field, 
    .mapsrc = mapsrc, 
    .maptar = maptar, 
    .source_root = source_root, 
    .target_root = target_root, 
    .size = size, 
    .corner[0] = (xmax + xmin - size) * 0.5, 
    .corner[1] = (ymax + ymin - size) * 0.5, 
    .corner[2] = (zmax + zmin - size) * 0.5
  }; 
  hpx_bcast(_init_param, &init_param_arg, sizeof(init_param_action_arg_t),
	    init_param_done); 
  hpx_lco_wait(init_param_done); 
  hpx_lco_delete(init_param_done, HPX_NULL); 

  /*
  // partition the source and target ensembles. On the source side,
  // when the partition reaches a leaf box, the source-to-multipole
  // action will be invoked immediately. 
  hpx_addr_t partition_done = hpx_lco_and_new(2); 
  char type1 = 'S', type2 = 'T'; 
  hpx_call(source_root, _partition_box, &type1, sizeof(type1), partition_done);
  hpx_call(target_root, _partition_box, &type2, sizeof(type2), partition_done); 
  hpx_lco_wait(partition_done); 
  hpx_lco_delete(partition_done, HPX_NULL); // HPX_NULL may be changed
					    // to an lco indicating
					    // work on target tree can start
					    */

  // Cleanup
  hpx_gas_global_free(sources, HPX_NULL);
  hpx_gas_global_free(charges, HPX_NULL);
  hpx_gas_global_free(targets, HPX_NULL);
  hpx_gas_global_free(potential, HPX_NULL);
  hpx_gas_global_free(field, HPX_NULL);
  hpx_gas_global_free(mapsrc, HPX_NULL); 
  hpx_gas_global_free(maptar, HPX_NULL); 
 
  hpx_shutdown(0);
}

init _init_sources_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  double *sources_p = NULL; 
  hpx_gas_try_pin(curr, (void **)&sources_p); 
  double bound[6] = {DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX};

  if (datatype == 1) {
    for (int i = 0; i < nsources; i++) {
      int j = 3 * i;
      sources_p[j]     = 1.0 * rand() / RAND_MAX - 0.5;
      sources_p[j + 1] = 1.0 * rand() / RAND_MAX - 0.5;
      sources_p[j + 2] = 1.0 * rand() / RAND_MAX - 0.5;
      
      bound[0] = fmin(bound[0], sources_p[j]); 
      bound[1] = fmax(bound[1], sources_p[j]); 
      bound[2] = fmin(bound[2], sources_p[j + 1]);
      bound[3] = fmax(bound[3], sources_p[j + 1]); 
      bound[4] = fmin(bound[4], sources_p[j + 2]); 
      bound[5] = fmax(bound[5], sources_p[j + 2]); 
    }
  } else if (datatype == 2) {
    double pi = acos(-1); 
    for (int i = 0; i < nsources; i++) {
      int j = 3 * i;
      double theta = 1.0*rand() / RAND_MAX * pi;
      double phi = 1.0*rand() / RAND_MAX * pi * 2;

      sources_p[j]     = sin(theta) * cos(phi);
      sources_p[j + 1] = sin(theta) * sin(phi);
      sources_p[j + 2] = cos(theta);
 
      bound[0] = fmin(bound[0], sources_p[j]); 
      bound[1] = fmax(bound[1], sources_p[j]); 
      bound[2] = fmin(bound[2], sources_p[j + 1]);
      bound[3] = fmax(bound[3], sources_p[j + 1]); 
      bound[4] = fmin(bound[4], sources_p[j + 2]); 
      bound[5] = fmax(bound[5], sources_p[j + 2]); 
   }
  }

  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(bound); 
  return HPX_SUCCESS;
} 

init _init_charges_action(void) {
  hpx_addr_t = curr = hpx_thread_current_target(); 
  double *charges_p = NULL;
  hpx_gas_try_pin(curr, (void **)&charges_p); 

  for (int i = 0; i < nsources; i++) 
    charges_p[i] = 1.0 * rand() / RAND_MAX - 0.5; 

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _init_targets_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  double *targets_p = NULL; 
  hpx_gas_try_pin(curr, (void **)&targets_p); 
  double bound[6] = {DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX};
  
  if (datatype == 1) {
    for (int i = 0; i < ntargets; i++) {
      int j = 3 * i;
      targets_p[j]     = 1.0 * rand() / RAND_MAX - 0.5;
      targets_p[j + 1] = 1.0 * rand() / RAND_MAX - 0.5;
      targets_p[j + 2] = 1.0 * rand() / RAND_MAX - 0.5;

      bound[0] = fmin(bound[0], sources_p[j]); 
      bound[1] = fmax(bound[1], sources_p[j]); 
      bound[2] = fmin(bound[2], sources_p[j + 1]);
      bound[3] = fmax(bound[3], sources_p[j + 1]); 
      bound[4] = fmin(bound[4], sources_p[j + 2]); 
      bound[5] = fmax(bound[5], sources_p[j + 2]); 
    }
  } else if (datatype == 2) {
    double pi = acos(-1);
    
    for (int i = 0; i < ntargets; i++) {
      int j = 3 * i;
      double theta = 1.0 * rand() / RAND_MAX * pi;
      double phi = 1.0 * rand() / RAND_MAX * pi * 2;
      targets_p[j]     = sin(theta) * cos(phi);
      targets_p[j + 1] = sin(theta) * sin(phi);
      targets_p[j + 2] = cos(theta);

      bound[0] = fmin(bound[0], sources_p[j]); 
      bound[1] = fmax(bound[1], sources_p[j]); 
      bound[2] = fmin(bound[2], sources_p[j + 1]);
      bound[3] = fmax(bound[3], sources_p[j + 1]); 
      bound[4] = fmin(bound[4], sources_p[j + 2]); 
      bound[5] = fmax(bound[5], sources_p[j + 2]); 
    }
  }
  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(bound); 
  return HPX_SUCCESS;
} 

int _init_mapsrc_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  int *mapsrc_p = NULL; 
  hpx_gas_try_pin(curr, (void **)&mapsrc_p); 

  for (int i = 0; i < nsources; i++) 
    mapsrc_p[i] = i;

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_maptar_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  int *maptar_p = NULL;
  hpx_gas_try_pin(curr, (void **)*maptar_p); 

  for (int i = 0; i < ntargets; i++)
    maptar_p[i] = i;

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_source_root_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *source_root_p = NULL; 
  hpx_gas_try_pin(curr, (void **)&source_root_p);

  source_root_p->level = 0; 
  source_root_p->index[0] = 0; 
  source_root_p->index[1] = 0; 
  source_root_p->index[2] = 0; 
  source_root_p->npts = nsources; 
  source_root_p->addr = 0; 

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_target_root_action(void *args) {
  hpx_addr_t list5_entry = *((hpx_addr_t *) args); 
  fmm_box_t *target_root_p = NULL;
  hpx_addr_t curr = hpx_thread_current_target(); 
  hpx_gas_try_pin(curr, (void **)&target_root_p); 

  target_root_p->level = 0; 
  target_root_p->index[0] = 0;
  target_root_p->index[1] = 0;
  target_root_p->index[2] = 0; 
  target_root_p->npts = ntargets; 
  target_root_p->addr = 0; 
  target_root_p->nlist5 = 1; 
  target_root_p->list5[0] = list5_entry; 

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_param_action(void *args) {
  init_param_action_arg_t *init_param_arg = (init_param_action_arg_t *) args; 

  sources = init_param_arg->sources; 
  charges = init_param_arg->charges; 
  targets = init_param_arg->targets; 
  potential = init_param_arg->potential; 
  field = init_param_arg->field; 
  mapsrc = init_param_arg->mapsrc; 
  maptar = init_param_arg->maptar; 
  source_root = init_param_arg->source_root; 
  target_root = init_param_arg->target_root; 

  fmm_param = calloc(1, sizeof(fmm_param_t)); 
  fmm_param-size = init_param_arg->size; 
  fmm_param->corner[0] = init_param_arg->corner[0]; 
  fmm_param->corner[1] = init_param_arg->corner[1]; 
  fmm_param->corner[2] = init_param_arg->corner[2]; 

  if (accuracy == 3) {
    fmm_param->pterms = 9;
    fmm_param->nlambs = 9;
    fmm_param->pgsz = 100;
  } else if (accuracy == 6) {
    fmm_param->pterms = 18;
    fmm_param->nlambs = 18;
    fmm_param->pgsz = 361;
  }

  int pterms = fmm_param->pterms;
  int nlambs = fmm_param->nlambs;
  int pgsz = fmm_param->pgsz;

  fmm_param->numphys = calloc(nlambs, sizeof(int));
  fmm_param->numfour = calloc(nlambs, sizeof(int));
  fmm_param->whts    = calloc(nlambs, sizeof(double));
  fmm_param->rlams   = calloc(nlambs, sizeof(double));
  fmm_param->rdplus  = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdminus = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdsq3   = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdmsq3  = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->dc = calloc((2 * pterms + 1)*(2 * pterms + 1)* (2 * pterms + 1),
			 sizeof(double));
  fmm_param->ytopc     = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->ytopcs    = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->ytopcsinv = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->rlsc      = calloc(pgsz * nlambs, sizeof(double));

  frmini(fmm_param);
  rotgen(fmm_param);
  vwts(fmm_param);
  numthetahalf(fmm_param);
  numthetafour(fmm_param);
  rlscini(fmm_param);

  fmm_param->nexptot  = 0;
  fmm_param->nthmax   = 0;
  fmm_param->nexptotp = 0;

  for (int i = 1; i <= nlambs; i++) {
    fmm_param->nexptot += fmm_param->numfour[i - 1];
    if (fmm_param->numfour[i - 1] > fmm_param->nthmax)
      fmm_param->nthmax = fmm_param->numfour[i - 1];
    fmm_param->nexptotp += fmm_param->numphys[i - 1];
  }

  fmm_param->nexptotp *= 0.5;
  fmm_param->nexpmax = (fmm_param->nexptot > fmm_param->nexptotp ?
			fmm_param->nexptot : fmm_param->nexptotp) + 1;

  fmm_param->xs = calloc(fmm_param->nexpmax * 3, sizeof(double complex));
  fmm_param->ys = calloc(fmm_param->nexpmax * 3, sizeof(double complex));
  fmm_param->zs = calloc(fmm_param->nexpmax * 3, sizeof(double));
  fmm_param->fexpe    = calloc(15000, sizeof(double complex));
  fmm_param->fexpo    = calloc(15000, sizeof(double complex));
  fmm_param->fexpback = calloc(15000, sizeof(double complex));

  mkfexp(fmm_param);
  mkexps(fmm_param);

  fmm_param->scale = calloc(MAXLEVEL, sizeof(double));
  fmm_param->scale[0] = 1 / _args->size;
  for (int i = 1; i <= MAXLEVEL; i++)
    fmm_param->scale[i] = 2 * fmm_param->scale[i - 1];
  return HPX_SUCCESS;
}


void lgndr(int nmax, double x, double *y) {
  int n;
  n = (nmax + 1) * (nmax + 1);
  for (int m = 0; m < n; m++)
    y[m] = 0.0;

  double u = -sqrt(1 - x * x);
  y[0] = 1;

  y[1] = x * y[0];
  for (int n = 2; n <= nmax; n++)
    y[n] = ((2 * n - 1) * x * y[n - 1] - (n - 1) * y[n - 2]) / n;

  int offset1 = nmax + 2;
  for (int m = 1; m <= nmax - 1; m++) {
    int offset2 = m * offset1;
    y[offset2] = y[offset2 - offset1] * u * (2 * m - 1);
    y[offset2 + 1] = y[offset2] * x * (2 * m + 1);
    for (int n = m + 2; n <= nmax; n++) {
      int offset3 = n + m * (nmax + 1);
      y[offset3] = ((2 * n - 1) * x * y[offset3 - 1] -
                    (n + m - 1) * y[offset3 - 2]) / (n - m);
    }
  }

  y[nmax + nmax * (nmax + 1)] =
    y[nmax - 1 + (nmax - 1) * (nmax + 1)] * u * (2 * nmax - 1);
}

void rotz2y(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param->pterms;
  int pgsz   = fmm_param->pgsz;

  double complex *mwork = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m =1; m <= pterms; m++)
    ephi[m] = -ephi[m - 1] * _Complex_I;

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      mwork[index] = ephi[m] * multipole[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mrotate[index] = mwork[ell] * rd[ell + (m + pterms) * pgsz];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp * (pterms + 1);
        mrotate[index] +=
          mwork[index1] * rd[ell + mp * (pterms + 1) + (m + pterms) * pgsz] +
          conj(mwork[index1]) *
          rd[ell + mp * (pterms + 1) + (pterms - m) * pgsz];
      }
    }
  }

  free(ephi);
  free(mwork);
}

void roty2z(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param->pterms;
  int pgsz   = fmm_param->pgsz;

  double complex *mwork = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m = 1; m <= pterms; m++)
    ephi[m] = ephi[m - 1] * _Complex_I;

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mwork[index] = multipole[ell] * rd[ell + (m + pterms) * pgsz];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp * (pterms + 1);
        double complex temp = multipole[index1];
        mwork[index] +=
          temp * rd[ell + mp * (pterms + 1) + (m + pterms) * pgsz] +
          conj(temp) * rd[ell + mp * (pterms + 1) + (pterms - m) * pgsz];
      }
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mrotate[index] = ephi[m] * mwork[index];
    }
  }

  free(ephi);
  free(mwork);
}

void rotz2x(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param->pterms;
  int pgsz   = fmm_param->pgsz;

  int offset1 = pterms * pgsz;
  for (int m = 0; m <= pterms; m++) {
    int offset2 = m * (pterms + 1);
    int offset3 = m * pgsz + offset1;
    int offset4 = -m * pgsz + offset1;
    for (int ell = m; ell <= pterms; ell++) {
      mrotate[ell + offset2] = multipole[ell] * rd[ell + offset3];
      for (int mp = 1; mp <= ell; mp++) {
        int offset5 = mp * (pterms + 1);
        mrotate[ell + offset2] +=
          multipole[ell + offset5] * rd[ell + offset3 + offset5] +
          conj(multipole[ell + offset5]) * rd[ell + offset4 + offset5];
      }
    }
  }
}

/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Implementations of FMM actions
/// ----------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fmm.h"

hpx_addr_t sources; 
hpx_addr_t charges; 
hpx_addr_t targets; 
hpx_addr_t potential;
hpx_addr_t field; 
hpx_addr_t source_root; 
hpx_addr_t target_root; 
hpx_addr_t mapsrc; 
hpx_addr_t maptar; 
hpx_addr_t init_param_done; 
hpx_addr_t partition_done; 

static double *sources_p = NULL; 
static double *charges_p = NULL;
static double *targets_p = NULL; 
static int *mapsrc_p = NULL;
static int *maptar_p = NULL;  

const int xoff[] = {0, 1, 0, 1, 0, 1, 0, 1};
const int yoff[] = {0, 0, 1, 1, 0, 0, 1, 1};
const int zoff[] = {0, 0, 0, 0, 1, 1, 1, 1};

int part_threshold; 

int _fmm_main_action(void *args) {
  fmm_config_t *fmm_cfg = (fmm_config_t *)args; 

  // generate test data according to the configuration
  int nsources = fmm_cfg->nsources; 
  int ntargets = fmm_cfg->ntargets; 
  int datatype = fmm_cfg->datatype; 

  // allocate memory to hold source and target info
  sources = hpx_gas_alloc(nsources * 3, sizeof(double)); 
  charges = hpx_gas_alloc(nsources, sizeof(double));
  targets = hpx_gas_alloc(ntargets * 3, sizeof(double)); 
  potential = hpx_gas_alloc(ntargets, sizeof(double));
  field = hpx_gas_alloc(ntargets * 3, sizeof(double));

  // pin the memory to populate test data
  hpx_gas_try_pin(sources, (void **)&sources_p);
  hpx_gas_try_pin(charges, (void **)&charges_p);
  hpx_gas_try_pin(targets, (void **)&targets_p); 

  double pi = acos(-1);

  if (datatype == 1) {
    for (int i = 0; i < nsources; i++) {
      int j = 3 * i;
      charges_p[i]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_p[j]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_p[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      sources_p[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3 * i;
      targets_p[j]     = 1.0*rand()/RAND_MAX - 0.5;
      targets_p[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      targets_p[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }
  } else if (datatype == 2) {
    for (int i = 0; i < nsources; i++) {
      int j = 3 * i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;

      charges_p[i]     = 1.0*rand()/RAND_MAX - 0.5;
      sources_p[j]     = sin(theta)*cos(phi);
      sources_p[j + 1] = sin(theta)*sin(phi);
      sources_p[j + 2] = cos(theta);
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3 * i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;
      targets_p[j]     = sin(theta)*cos(phi);
      targets_p[j + 1] = sin(theta)*sin(phi);
      targets_p[j + 2] = cos(theta);
    }
  }

  // find the smallest box enclosing all the points 
  double xmin = sources_p[0], xmax = xmin; 
  double ymin = sources_p[1], ymax = ymin;
  double zmin = sources_p[2], zmax = zmin; 

  for (int i = 1; i < nsources; i++) {
    int j = 3 * i; 
    xmin = fmin(sources_p[j],     xmin); 
    xmax = fmax(sources_p[j],     xmax); 
    ymin = fmin(sources_p[j + 1], ymin); 
    ymax = fmax(sources_p[j + 1], ymax); 
    zmin = fmin(sources_p[j + 2], zmin); 
    zmax = fmax(sources_p[j + 2], zmax); 
  }

  for (int i = 0; i < ntargets; i++) {
    int j = 3 * i;
    xmin = fmin(targets_p[j],     xmin);
    xmax = fmax(targets_p[j],     xmax);
    ymin = fmin(targets_p[j + 1], ymin);
    ymax = fmax(targets_p[j + 1], ymax);
    zmin = fmin(targets_p[j + 2], zmin);
    zmax = fmax(targets_p[j + 2], zmax);
  }

  // construct FMM param on each locality
  const int n_localities = hpx_get_num_ranks(); 
  init_param_done = hpx_lco_and_new(n_localities); 

  for (int i = 0; i < n_localities; i++) {
    static init_param_action_arg_t init_param_arg; 
    init_param_arg.accuracy = fmm_cfg->accuracy; 
    init_param_arg.size = fmax(fmax(xmax - xmin, ymax - ymin), zmax - zmin);
    init_param_arg.corner[0] = (xmax + xmin - init_param_arg.size) * 0.5;
    init_param_arg.corner[1] = (ymax + ymin - init_param_arg.size) * 0.5;
    init_param_arg.corner[2] = (zmax + zmin - init_param_arg.size) * 0.5;

    hpx_call(HPX_THERE(i), _init_param, &init_param_arg, 
	     sizeof(init_param_action_arg_t), init_param_done);
  }

  hpx_lco_wait(init_param_done); 
  hpx_lco_delete(init_param_done, HPX_NULL); 

  // allocate memory to hold mapping info
  mapsrc = hpx_gas_alloc(nsources, sizeof(int)); 
  maptar = hpx_gas_alloc(ntargets, sizeof(int)); 

  hpx_gas_try_pin(mapsrc, (void *)&mapsrc_p); 
  hpx_gas_try_pin(maptar, (void *)&maptar_p); 

  // initialize mapping 
  for (int i = 0; i < nsources; i++) 
    mapsrc_p[i] = i; 

  for (int i = 0; i < ntargets; i++) 
    maptar_p[i] = i; 

  // allocate memory to hold roots of the source and target trees
  source_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); // expansion is NULL
  target_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); 
  partition_done = hpx_lco_and_new(2); // and-gate

  // set up the root node
  fmm_box_t *source_root_p = NULL;
  fmm_box_t *target_root_p = NULL;
  hpx_gas_try_pin(source_root, (void **)&source_root_p);
  hpx_gas_try_pin(target_root, (void **)&target_root_p);

  source_root_p->level = 0; 
  source_root_p->idx = 0; 
  source_root_p->idy = 0;
  source_root_p->idz = 0;
  source_root_p->npts = nsources; 
  source_root_p->addr = 0; 

  target_root_p->level = 0; 
  target_root_p->idx = 0; 
  target_root_p->idy = 0;
  target_root_p->idz = 0;
  target_root_p->npts = ntargets; 
  target_root_p->addr = 0; 

  hpx_gas_unpin(source_root);
  hpx_gas_unpin(target_root); 

  // partition the source and target ensembles. On the source side,
  // when the partition reaches a leaf box, source-to-multipole action
  // is invoked immediately. 
  part_threshold = fmm_cfg->s;   
  char type1 = 'S', type2 = 'T'; 
  hpx_call(source_root, _partition_box, &type1, sizeof(type1), partition_done);
  hpx_call(target_root, _partition_box, &type2, sizeof(type2), partition_done); 
  hpx_lco_wait(partition_done); 

  // source and target info, and the mappings remain pinned during computation 
  hpx_gas_unpin(mapsrc);
  hpx_gas_unpin(maptar); 
  hpx_gas_unpin(sources); 
  hpx_gas_unpin(charges); 
  hpx_gas_unpin(targets);
  
  // cleanup 
  hpx_gas_global_free(sources); 
  hpx_gas_global_free(charges);
  hpx_gas_global_free(targets);
  hpx_gas_global_free(potential);
  hpx_gas_global_free(field); 
  
  hpx_shutdown(0); 
}

int _init_param_action(void *args) {
  init_param_action_arg_t *_args = (init_param_action_arg_t *)args; 

  fmm_param = calloc(1, sizeof(fmm_param_t)); 

  fmm_param->size = _args->size; 
  fmm_param->corner[0] = _args->corner[0]; 
  fmm_param->corner[1] = _args->corner[1]; 

  int accuracy = _args->accuracy; 

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

int _partition_box_action(void *args) {
  const char type = *((char *) args);
  fmm_box_t *box = NULL; 

  hpx_gas_try_pin(hpx_thread_current_target(), (void **)&box); 

  int begin = box->addr; 
  int npoints  = box->npts; 
  int *swap = calloc(npoints, sizeof(int)); 
  int *record = calloc(npoints, sizeof(int)); 
  int *imap = (type == 'S' ? &mapsrc_p[begin] : &maptar_p[begin]); 
  double *points = (type == 'S' ? sources_p : targets_p); 

  // compute coordinates of the box center 
  double h = fmm_param->size / (1 << (box->level + 1)); 
  double xc = fmm_param->corner[0] + (2 * box->idx + 1) * h; 
  double yc = fmm_param->corner[1] + (2 * box->idy + 1) * h;
  double zc = fmm_param->corner[2] + (2 * box->idz + 1) * h; 

  unsigned child_parts[8] = {0}, addrs[8] = {0}, assigned[8] = {0}; 

  for (int i = 0; i < npoints; i++) {
    int j = 3 * imap[i]; 
    int bin = 4 * (points[j + 2] > zc) + 2 * (points[j + 1] > yc) + 
      (points[j] > xc); 
    record[i] = bin;
  }

  for (int i = 0; i < npoints; i++) 
    child_parts[record[i]]++;

  addrs[1] = addrs[0] + child_parts[0]; 
  addrs[2] = addrs[1] + child_parts[1]; 
  addrs[3] = addrs[2] + child_parts[2]; 
  addrs[4] = addrs[3] + child_parts[3]; 
  addrs[5] = addrs[4] + child_parts[4]; 
  addrs[6] = addrs[5] + child_parts[5]; 
  addrs[7] = addrs[6] + child_parts[6]; 

  // assign points to boxes
  for (int i = 0; i < npoints; i++) {
    int bin = record[i]; 
    int offset = addrs[bin] + assigned[bin]++; 
    swap[offset] = imap[i];
  }

  for (int i = 0; i < npoints; i++) 
    imap[i] = swap[i];

  box->nchild = (child_parts[0] > 0) + (child_parts[1] > 0) + 
    (child_parts[2] > 0) + (child_parts[3] > 0) + (child_parts[4] > 0) + 
    (child_parts[5] > 0) + (child_parts[6] > 0) + (child_parts[7] > 0); 

  // create new boxes if necessary
  int buffer_size = sizeof(double complex) * 
    (fmm_param->pgsz + 6 * fmm_param->nexpmax * (type == 'S')); 
  int and_gate_size = 0; 
  
  for (int i = 0; i < 8; i++) {
    if (child_parts[i] > 0) {
      box->child[i] = hpx_gas_alloc(1, sizeof(fmm_box_t) + buffer_size); 
      fmm_box_t *cbox = NULL;
      hpx_gas_try_pin(box->child[i], (void **)&cbox); 

      cbox->level = box->level + 1;
      cbox->parent = hpx_thread_current_target(); 
      cbox->idx = 2 * box->idx + xoff[i]; 
      cbox->idy = 2 * box->idy + yoff[i];
      cbox->idz = 2 * box->idz + zoff[i]; 
      cbox->npts = child_parts[i]; 
      cbox->addr = box->addr + addrs[i]; 

      and_gate_size += (child_parts[i] > part_threshold); 
    }
  }

  if (type == 'S') {
    box->n_reduce = box->nchild; 
    box->sema = hpx_lco_sema_new(1); 
  }

  if (and_gate_size) {
    hpx_addr_t branching = hpx_lco_and_new(and_gate_size); 
    for (int i = 0; i < 8; i++) {
      if (child_parts[i] > part_threshold) 
	hpx_call(box->child[i], _partition_box, &type, sizeof(type), branching);
    }

    hpx_lco_wait(branching);
    hpx_lco_delete(branching, HPX_NULL);
  } else {
    // the new child boxes are all leaf boxes, spawn source-to-mutlipole in a
    // fire-and-forget way 
    for (int i = 0; i < 8; i++) {
      if (child_parts[i]) 
	hpx_call(box->child[i], _source_to_mpole, NULL, 0, HPX_NULL); 
    }
  }

  return HPX_SUCCESS;
}

int _source_to_multipole_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  if (!hpx_gas_try_pin(curr, (void **)&sbox))
    return HPX_RESEND; 

  int pgsz      = fmm_param->pgsz; 
  int pterms    = fmm_param->pterms; 
  double *ytopc = fmm_param->ytopc;
  int addr      = sbox->addr; 
  int level     = sbox->level; 

  double complex *multipole = &sbox->expansion[0]; 
  double *sources = &sources_p[3 * addr]; 
  double *charges = &charges_p[addr]; 
  int nsources = sbox->npts; 
  double myscale = fmm_param->scale[level]; 
  double h = fmm_param->size / (1 << (level + 1)); 
  double center[3]; 
  center[0] = fmm_param->corner[0] + (2 * sbox->idx + 1) * h; 
  center[1] = fmm_param->corner[1] + (2 * sbox->idy + 1) * h;
  center[2] = fmm_param->corner[2] + (2 * sbox->idz + 1) * h; 

  const double precision = 1e-14;
  double *powers = calloc(pterms + 1, sizeof(double));
  double *p = calloc(pgsz, sizeof(double));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));

  for (int i = 0; i < nsources; i++) {
    int i3 = i * 3; 
    double rx = sources[i3]     - center[0];
    double ry = sources[i3 + 1] - center[1];
    double rz = sources[i3 + 2] - center[2];
    double proj = rx * rx + ry * ry;
    double rr = proj + rz * rz;
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 1.0 : rz / d);
    ephi[0] = (proj <= precision * d ? 1.0 : rx / proj + _Complex_I * ry / proj);
    d *= myscale;
    powers[0] = 1.0;
    for (int ell = 1; ell <= pterms; ell++) {
      powers[ell] = powers[ell - 1] * d;
      ephi[ell] = ephi[ell - 1] * ephi[0];
    }

    double charge = charges[i];
    multipole[0] += charge;

    lgndr(pterms, ctheta, p);
    for (int ell = 1; ell <= pterms; ell++) {
      double cp = charge * powers[ell] * p[ell];
      multipole[ell] += cp;
    }

    for (int m = 1; m <= pterms; m++) {
      int offset1 = m * (pterms + 1);
      int offset2 = m * (pterms + 2); 
      for (int ell = m; ell <= pterms; ell++) {
        double cp = charge * powers[ell] * ytopc[ell + offset2] * 
          p[ell + offset1];
        multipole[ell + offset1] += cp * conj(ephi[m - 1]);
      }
    }
  }

  free(powers);
  free(p);
  free(ephi);

  // spawn one thread to continue on multipole-to-multipole translation and three
  // other threads on multipole-to-exponential translations, all in
  // fire-and-forget way
  int ichild = (sbox->idz % 2) * 4 + (sbox->idy % 2) * 2 + (sbox->idx % 2); 
  const char dir[3] = {'z', 'y', 'x'};  
  hpx_gas_unpin(curr); 
  hpx_call(curr, _mpole_to_mpole, &ichild, sizeof(ichild), HPX_NULL); 
  hpx_call(curr, _mpole_to_expo, &dir[0], sizeof(char), HPX_NULL); 
  hpx_call(curr, _mpole_to_expo, &dir[1], sizeof(char), HPX_NULL);
  hpx_call(curr, _mpole_to_expo, &dir[2], sizeof(char), HPX_NULL);
  return HPX_SUCCESS; 
}

int _multipole_to_multipole_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  if (!hpx_gas_try_pin(curr, (void **)&sbox))
    return HPX_RESEND; 

  int ichild = *((int *) args); 
  const double complex var[5] =
    {1,-1 + _Complex_I, 1 + _Complex_I, 1 - _Complex_I, -1 - _Complex_I};
  const double arg = sqrt(2)/2.0;
  const int iflu[8] = {3, 4, 2, 1, 3, 4, 2, 1};

  int pterms = fmm_param->pterms;
  int pgsz   = fmm_param->pgsz;
  double *dc = fmm_param->dc;

  double *powers = calloc(pterms + 3, sizeof(double));
  double complex *mpolen = calloc(pgsz, sizeof(double complex));
  double complex *marray = calloc(pgsz, sizeof(double complex));
  double complex *ephi   = calloc(pterms + 3, sizeof(double complex));
  
  int ifl = iflu[ichild]; 
  double *rd = (ichild < 4 ? fmm_param->rdsq3 : fmm_param->rdmsq3); 
  double complex *mpole = &sbox->expansion[0]; 

  ephi[0] = 1.0;
  ephi[1] = arg * var[ifl];
  double dd = -sqrt(3) / 2.0;
  powers[0] = 1.0;

  for (int ell = 1; ell <= pterms + 1; ell++) {
    powers[ell] = powers[ell - 1] * dd;
    ephi[ell + 1] = ephi[ell] * ephi[1];
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset; 
      mpolen[index] = conj(ephi[m]) * mpole[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    int offset1 = (m + pterms) * pgsz;
    int offset2 = (-m + pterms) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      marray[index] = mpolen[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp++) {
	int index1 = ell + mp * (pterms + 1);
	marray[index] += mpolen[index1] * rd[index1 + offset1] +
	  conj(mpolen[index1]) * rd[index1 + offset2];
      }
    }
  }
  
  for (int k = 0; k <= pterms; k++) {
    int offset = k * (pterms + 1);
    for (int j = k; j <= pterms; j++) {
      int index = offset + j;
      mpolen[index] = marray[index];
      for (int ell = 1; ell <= j - k; ell++) {
	int index2 = j - k + ell * (2 * pterms + 1);
	int index3 = j + k + ell * (2 * pterms + 1);
	mpolen[index] += marray[index - ell] * powers[ell] *
	  dc[index2] * dc[index3];
      }
    }
  }
  
  for (int m = 0; m <= pterms; m += 2) {
    int offset = m * (pterms + 1);
    int offset1 = (m + pterms) * pgsz;
    int offset2 = (-m + pterms) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = mpolen[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
	int index1 = ell + mp * (pterms + 1);
	marray[index] -= mpolen[index1] * rd[index1 + offset1] +
	  conj(mpolen[index1]) * rd[index1 + offset2];
      }
      
      for (int mp = 2; mp <= ell; mp += 2) {
	int index1 = ell + mp * (pterms + 1);
	marray[index] += mpolen[index1] * rd[index1 + offset1] +
	  conj(mpolen[index1]) * rd[index1 + offset2];
      }
    }
  }

  for (int m = 1; m <= pterms; m += 2) {
    int offset = m * (pterms + 1);
    int offset1 = (m + pterms) * pgsz;
    int offset2 = (-m + pterms) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = -mpolen[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
	int index1 = ell + mp * (pterms + 1);
	marray[index] += mpolen[index1] * rd[index1 + offset1] +
	  conj(mpolen[index1]) * rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
	int index1 = ell + mp * (pterms + 1);
	marray[index] -= mpolen[index1] * rd[index1 + offset1] +
	  conj(mpolen[index1]) * rd[index1 + offset2];
      }
    }
  }
  
  powers[0] = 1.0;
  for (int ell = 1; ell <= pterms + 1; ell++)
    powers[ell] = powers[ell - 1] / 2;
  
  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mpolen[index] = ephi[m] * marray[index] * powers[ell];
    }
  }

  hpx_gas_unpin(curr); 
  hpx_call(sbox->parent, _mpole_reduction, mpolen, 
	   sizeof(double complex) * pgsz, HPX_NULL); 

  free(ephi);
  free(powers);
  free(mpolen);
  free(marray); 

  return HPX_SUCCESS; 
}

int _multipole_reduction_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *box = NULL; 
  if (!hpx_gas_try_pin(curr, (void **)&box))
    return HPX_RESEND; 

  double complex *input  = (double complex *)args; 
  double complex *output = &box->expansion[0]; 
  hpx_addr_t sema = box->sema; 
  int pgsz = fmm_param->pgsz; 
  bool last_arrival = false; 

  while (hpx_lco_sema_p(sema)) {
    for (int i = 0; i < pgsz; i++) 
      output[i] += input[i]; 
    last_arrival = (--box->n_reduce == 0); 
  }

  hpx_lco_sema_v(sema); 

  hpx_gas_unpin(curr); 
  
  if (last_arrival) {
    // the thread arrives last at the parent box spawn four more actions
    int ichild = (box->idz % 2) * 4 + (box->idy % 2) * 2 + (box->idx % 2); 
    const char dir[3] = {'z', 'y', 'x'}; 
    hpx_call(curr, _mpole_to_mpole, &ichild, sizeof(ichild), HPX_NULL); 
    hpx_call(curr, _mpole_to_expo, &dir[0], sizeof(char), HPX_NULL);
    hpx_call(curr, _mpole_to_expo, &dir[1], sizeof(char), HPX_NULL);
    hpx_call(curr, _mpole_to_expo, &dir[2], sizeof(char), HPX_NULL);
  }

  return HPX_SUCCESS; 
}

int _multipole_to_exponential_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL;   
  if (!hpx_gas_try_pin(curr, (void **)&sbox))
    return HPX_RESEND; 

  char dir        = *((char *) args); 
  int pgsz        = fmm_param->pgsz; 
  int nexpmax     = fmm_param->nexpmax; 
  double *rdminus = fmm_param->rdminus; 
  double *rdplus  = fmm_param->rdplus; 

  double complex *mw = calloc(pgsz, sizeof(double complex)); 
  double complex *mexpf1 = calloc(nexpmax, sizeof(double complex)); 
  double complex *mexpf2 = calloc(nexpmax, sizeof(double complex)); 

  switch (dir) {
  case 'z': 
    multipole_to_exponential_p1(&sbox->expansion[0], mexpf1, mexpf2); 
    multipole_to_exponential_p2(mexpf1, &sbox->expansion[pgsz]); 
    multipole_to_exponential_p2(mexpf2, &sbox->expansion[pgsz + nexpmax]);
    break;
  case 'y':
    rotz2y(&sbox->expansion[0], rdminus, mw); 
    multipole_to_exponential_p1(mw, mexpf1, mexpf2); 
    multipole_to_exponential_p2(mexpf1, &sbox->expansion[pgsz + nexpmax * 2]); 
    multipole_to_exponential_p2(mexpf2, &sbox->expansion[pgsz + nexpmax * 3]); 
    break; 
  case 'x':
    rotz2x(&sbox->expansion[0], rdplus, mw); 
    multipole_to_exponential_p1(mw, mexpf1, mexpf2); 
    multipole_to_exponential_p2(mexpf1, &sbox->expansion[pgsz + nexpmax * 4]); 
    multipole_to_exponential_p2(mexpf2, &sbox->expansion[pgsz + nexpmax * 5]); 
    break; 
  default: 
    break;
  } 

  hpx_gas_unpin(curr); 
  free(mw); 
  free(mexpf1); 
  free(mexpf2); 
  return HPX_SUCCESS; 
}

void multipole_to_exponential_p1(const double complex *multipole, 
                                 double complex *mexpu, 
                                 double complex *mexpd) {
  int nlambs   = fmm_param->nlambs;
  int *numfour = fmm_param->numfour;
  int pterms   = fmm_param->pterms;
  int pgsz     = fmm_param->pgsz;
  double *rlsc = fmm_param->rlsc;

  int ntot = 0;
  for (int nell = 0; nell < nlambs; nell++) {
    double sgn = -1.0;
    double complex zeyep = 1.0;
    for (int mth = 0; mth <= numfour[nell] - 1; mth++) {
      int ncurrent = ntot + mth;
      double complex ztmp1 = 0.0;
      double complex ztmp2 = 0.0;
      sgn = -sgn;
      int offset = mth * (pterms + 1);
      int offset1 = offset + nell * pgsz;
      for (int nm = mth; nm <= pterms; nm += 2)
        ztmp1 += rlsc[nm + offset1] * multipole[nm + offset];
      for (int nm = mth + 1; nm <= pterms; nm += 2)
        ztmp2 += rlsc[nm + offset1] * multipole[nm + offset];

      mexpu[ncurrent] = (ztmp1 + ztmp2) * zeyep;
      mexpd[ncurrent] = sgn * (ztmp1 - ztmp2) * zeyep;
      zeyep *= _Complex_I;
    }
    ntot += numfour[nell];
  }
}

void multipole_to_exponential_p2(const double complex *mexpf, 
                                 double complex *mexpphys) {
  int nlambs   = fmm_param->nlambs;
  int *numfour = fmm_param->numfour;
  int *numphys = fmm_param->numphys;

  int nftot, nptot, nexte, nexto;
  nftot = 0;
  nptot = 0;
  nexte = 0;
  nexto = 0;

  double complex *fexpe = fmm_param->fexpe;
  double complex *fexpo = fmm_param->fexpo;

  for (int i = 0; i < nlambs; i++) {
    for (int ival = 0; ival < numphys[i] / 2; ival++) {
      mexpphys[nptot + ival] = mexpf[nftot]; 

      for (int nm = 1; nm < numfour[i]; nm += 2) {
        double rt1 = cimag(fexpe[nexte]) * creal(mexpf[nftot + nm]);
        double rt2 = creal(fexpe[nexte]) * cimag(mexpf[nftot + nm]);
        double rtmp = 2 * (rt1 + rt2);

        nexte++;
        mexpphys[nptot + ival] += rtmp * _Complex_I;
      }

      for (int nm = 2; nm < numfour[i]; nm += 2) {
        double rt1 = creal(fexpo[nexto]) * creal(mexpf[nftot + nm]);
        double rt2 = cimag(fexpo[nexto]) * cimag(mexpf[nftot + nm]);
        double rtmp = 2 * (rt1 - rt2);

        nexto++;
        mexpphys[nptot + ival] += rtmp;
      }
    }
    nftot += numfour[i];
    nptot += numphys[i]/2;
  }
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

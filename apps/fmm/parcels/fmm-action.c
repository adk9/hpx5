/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Implementations of FMM actions
/// ----------------------------------------------------------------------------

#include <math.h>
#include <stdlib.h>
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

static void init_double_complex(double complex *input, const size_t size) {
  assert(size % sizeof(double complex) == 0); 
  const int n_entries = size / sizeof(double complex); 
  for (int i = 0; i < n_entries; i++) 
    input[i] = 0; 
}

static void sum_double_complex(double complex *output, 
			       const double complex *input, 
			       const size_t size) {
  assert(size % sizeof(double complex) == 0); 
  const int n_entries = size / sizeof(double complex); 
  for (int i = 0; i < n_entries; i++) 
    output[i] += input[i]; 
}

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

  // partition the source and target ensembles. On the source side, when the
  // partition reaches to a leaf box, source-to-multipole action is invoked
  // immediately. The target side needs to wait for completion of the partition
  // in order to construct (and possibly trimming) lists. 
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
    box->reduce = ///< handle multipole-to-multipole reduction
      hpx_lco_allreduce_new(box->nchild, 
			    sizeof(double complex) * fmm_param->pgsz, 
			    (hpx_commutative_associative_op_t) sum_double_complex, 
			    (void (*)(void *, const size_t)) init_double_complex); 
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
	hpx_call(box->child[i], _source_to_multipole, NULL, 0, HPX_NULL); 
    }
  }

  return HPX_SUCCESS;
}

int _source_to_multipole_action(void) {
  return HPX_SUCCESS; 
}

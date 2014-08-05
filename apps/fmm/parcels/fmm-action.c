/// ---------------------------------------------------------------------------
/// @file fmm-action.c
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Implementations of FMM actions
/// ---------------------------------------------------------------------------
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include "fmm.h"

const int xoff[] = {0, 1, 0, 1, 0, 1, 0, 1};
const int yoff[] = {0, 0, 1, 1, 0, 0, 1, 1};
const int zoff[] = {0, 0, 0, 0, 1, 1, 1, 1};

int _fmm_main_action(void) {
  // Allocate memory to hold source and target information
  hpx_addr_t sources = hpx_gas_alloc(nsources, sizeof(source_t)); 
  hpx_addr_t targets = hpx_gas_alloc(ntargets, sizeof(target_t)); 

  // Populate test data
  hpx_addr_t bound_src = hpx_lco_future_new(sizeof(double) * 6); 
  hpx_addr_t bound_tar = hpx_lco_future_new(sizeof(double) * 6); 
  hpx_call(sources, _init_sources, NULL, 0, bound_src); 
  hpx_call(targets, _init_targets, NULL, 0, bound_tar); 

  // Determine the smallest bounding box 
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

  // Construct root nodes of the source and target trees
  hpx_addr_t roots_done = hpx_lco_and_new(2); 
  hpx_addr_t source_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); 
  hpx_addr_t target_root = hpx_gas_alloc(1, sizeof(fmm_box_t)); 
  hpx_call(source_root, _init_source_root, NULL, 0, roots_done); 
  hpx_call(target_root, _init_target_root, NULL, 0, roots_done); 
  hpx_lco_wait(roots_done); 
  hpx_lco_delete(roots_done, HPX_NULL); 

  hpx_addr_t sema_done = hpx_lco_sema_new(1); 
  hpx_addr_t fmm_done = hpx_lco_future_new(0); 

  // Construct FMM param on each locality
  hpx_addr_t params_done = hpx_lco_future_new(0); 
  init_param_action_arg_t init_param_arg = {
    .sources = sources, 
    .targets = targets, 
    .source_root = source_root, 
    .target_root = target_root, 
    .sema_done = sema_done, 
    .fmm_done = fmm_done, 
    .size = size, 
    .corner[0] = (xmax + xmin - size) * 0.5, 
    .corner[1] = (ymax + ymin - size) * 0.5, 
    .corner[2] = (zmax + zmin - size) * 0.5
  }; 
  hpx_bcast(_init_param, &init_param_arg, sizeof(init_param_action_arg_t), 
	    params_done); 
  hpx_lco_wait(params_done); 
  hpx_lco_delete(params_done, HPX_NULL); 
  
  // Partition the source and target ensemble. On the source part, the 
  // aggregate action is invoked immediately when a leaf is reached
  hpx_addr_t partition_done = hpx_lco_and_new(2); 
  char type1 = 'S', type2 = 'T'; 
  hpx_call(source_root, _partition_box, &type1, sizeof(type1), partition_done); 
  hpx_call(target_root, _partition_box, &type2, sizeof(type2), partition_done); 
  hpx_lco_wait(partition_done); 
  hpx_lco_delete(partition_done, HPX_NULL); 
  
  // Spawn disaggregate action along the target tree
  hpx_call(target_root, _disaggregate, NULL, 0, HPX_NULL); 

  // Wait for completion
  hpx_lco_wait(fmm_param->fmm_done); 
  
  // Cleanup
  hpx_gas_global_free(sources, HPX_NULL);
  hpx_gas_global_free(targets, HPX_NULL);
  
  hpx_shutdown(0);
}

int _init_sources_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  source_t *sources_p = NULL; 
  hpx_gas_try_pin(curr, (void **)&sources_p);
  double bound[6] = {DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX};
  
  if (datatype == 1) {
    for (int i = 0; i < nsources; i++) {
      double x = 1.0 * rand() / RAND_MAX - 0.5; 
      double y = 1.0 * rand() / RAND_MAX - 0.5; 
      double z = 1.0 * rand() / RAND_MAX - 0.5; 
      double q = 1.0 * rand() / RAND_MAX - 0.5; 

      bound[0] = fmin(bound[0], x); 
      bound[1] = fmax(bound[1], x); 
      bound[2] = fmin(bound[2], y);
      bound[3] = fmax(bound[3], y);
      bound[4] = fmin(bound[4], z);
      bound[5] = fmax(bound[5], z); 

      sources_p[i].position[0] = x;
      sources_p[i].position[1] = y;
      sources_p[i].position[2] = z;
      sources_p[i].charge = q; 
      sources_p[i].rank   = i;
    }
  } else if (datatype == 2) {
    double pi = acos(-1); 
    for (int i = 0; i < nsources; i++) {
      double theta = 1.0*rand() / RAND_MAX * pi;
      double phi = 1.0*rand() / RAND_MAX * pi * 2;
      double x = sin(theta) * cos(phi); 
      double y = sin(theta) * sin(phi); 
      double z = cos(theta); 
      double q = 1.0 * rand() / RAND_MAX - 0.5; 
      
      bound[0] = fmin(bound[0], x); 
      bound[1] = fmax(bound[1], x); 
      bound[2] = fmin(bound[2], y);
      bound[3] = fmax(bound[3], y);
      bound[4] = fmin(bound[4], z);
      bound[5] = fmax(bound[5], z); 

      sources_p[i].position[0] = x;
      sources_p[i].position[1] = y;
      sources_p[i].position[2] = z;
      sources_p[i].charge = q; 
      sources_p[i].rank   = i;
    }
  }
  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(bound); 
  return HPX_SUCCESS;
} 

int _init_targets_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  target_t *targets_p = NULL;
  hpx_gas_try_pin(curr, (void **)&targets_p); 
  double bound[6] = {DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX};
  
  if (datatype == 1) {
    for (int i = 0; i < ntargets; i++) {
      double x = 1.0 * rand() / RAND_MAX - 0.5; 
      double y = 1.0 * rand() / RAND_MAX - 0.5; 
      double z = 1.0 * rand() / RAND_MAX - 0.5; 

      bound[0] = fmin(bound[0], x); 
      bound[1] = fmax(bound[1], x); 
      bound[2] = fmin(bound[2], y);
      bound[3] = fmax(bound[3], y);
      bound[4] = fmin(bound[4], z);
      bound[5] = fmax(bound[5], z); 

      targets_p[i].position[0] = x;
      targets_p[i].position[1] = y;
      targets_p[i].position[2] = z;
      targets_p[i].potential = 0; 
      targets_p[i].field[0] = 0; 
      targets_p[i].field[1] = 0;
      targets_p[i].field[2] = 0; 
      targets_p[i].rank = i; 
    }
  } else if (datatype == 2) {
    double pi = acos(-1);    
    for (int i = 0; i < ntargets; i++) {
      double theta = 1.0 * rand() / RAND_MAX * pi;
      double phi = 1.0 * rand() / RAND_MAX * pi * 2;

      double x = sin(theta) * cos(phi); 
      double y = sin(theta) * sin(phi); 
      double z = cos(theta); 
      
      bound[0] = fmin(bound[0], x); 
      bound[1] = fmax(bound[1], x); 
      bound[2] = fmin(bound[2], y);
      bound[3] = fmax(bound[3], y);
      bound[4] = fmin(bound[4], z);
      bound[5] = fmax(bound[5], z); 

      targets_p[i].position[0] = x;
      targets_p[i].position[1] = y;
      targets_p[i].position[2] = z;
      targets_p[i].potential = 0; 
      targets_p[i].field[0] = 0; 
      targets_p[i].field[1] = 0;
      targets_p[i].field[2] = 0; 
      targets_p[i].rank = i; 
    }
  }
  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(bound); 
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
  source_root_p->expan_avail = hpx_lco_and_new(3); 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_target_root_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *target_root_p = NULL;
  hpx_gas_try_pin(curr, (void **)&target_root_p); 
  target_root_p->level = 0; 
  target_root_p->index[0] = 0;
  target_root_p->index[1] = 0;
  target_root_p->index[2] = 0; 
  target_root_p->npts = ntargets; 
  target_root_p->addr = 0; 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _init_param_action(void *args) {
  init_param_action_arg_t *init_param_arg = (init_param_action_arg_t *) args; 
  
  fmm_param = calloc(1, sizeof(fmm_param_t)); 
  fmm_param->sources = init_param_arg->sources; 
  fmm_param->targets = init_param_arg->targets; 
  fmm_param->source_root = init_param_arg->source_root; 
  fmm_param->target_root = init_param_arg->target_root; 
  fmm_param->sema_done = init_param_arg->sema_done; 
  fmm_param->fmm_done = init_param_arg->fmm_done; 
  fmm_param->size = init_param_arg->size; 
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
  fmm_param->whts = calloc(nlambs, sizeof(double));
  fmm_param->rlams = calloc(nlambs, sizeof(double));
  fmm_param->rdplus = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdminus = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdsq3 = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->rdmsq3 = calloc(pgsz * (2 * pterms + 1), sizeof(double));
  fmm_param->dc = calloc((2 * pterms + 1)*(2 * pterms + 1)* (2 * pterms + 1), 
			 sizeof(double));
  fmm_param->ytopc = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->ytopcs = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->ytopcsinv = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  fmm_param->rlsc = calloc(pgsz * nlambs, sizeof(double));

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
  fmm_param->nexpmax = fmax(fmm_param->nexptot, fmm_param->nexptotp) + 1; 
  fmm_param->ys = calloc(fmm_param->nexpmax * 3, sizeof(double complex));
  fmm_param->zs = calloc(fmm_param->nexpmax * 3, sizeof(double));
  fmm_param->fexpe = calloc(15000, sizeof(double complex));
  fmm_param->fexpo = calloc(15000, sizeof(double complex));
  fmm_param->fexpback = calloc(15000, sizeof(double complex));

  mkfexp(fmm_param);
  mkexps(fmm_param);

  fmm_param->scale = calloc(MAXLEVEL, sizeof(double));
  fmm_param->scale[0] = 1 / init_param_arg->size;
  for (int i = 1; i <= MAXLEVEL; i++)
    fmm_param->scale[i] = 2 * fmm_param->scale[i - 1];
  return HPX_SUCCESS;
}

int _partition_box_action(void *args) {
  const char type = *((char *) args); 
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *box = NULL; 
  hpx_gas_try_pin(curr, (void *)&box); 

  swap_action_arg_t temp = {
    .type = type, 
    .addr = box->addr, 
    .npts = box->npts, 
    .level = box->level, 
    .index[0] = box->index[0], 
    .index[1] = box->index[1], 
    .index[2] = box->index[2]
  }; 

  hpx_addr_t points = (type == 'S' ? fmm_param->sources : fmm_param->targets); 
  hpx_addr_t partition = hpx_lco_future_new(sizeof(int) * 16); 
  hpx_call(points, _swap, &temp, sizeof(swap_action_arg_t), partition); 

  int result[16] = {0}, *subparts = &result[0], *addrs = &result[8]; 
  hpx_lco_get(partition, sizeof(int) * 16, result); 
  hpx_lco_delete(partition, HPX_NULL); 

  box->nchild = (subparts[0] > 0) + (subparts[1] > 0) + (subparts[2] > 0) + 
    (subparts[3] > 0) + (subparts[4] > 0) + (subparts[5] > 0) + 
    (subparts[6] > 0) + (subparts[7] > 0); 

  hpx_addr_t branch = hpx_lco_and_new(box->nchild); 
  int expansion_size = sizeof(double complex) * 
    (fmm_param->pgsz + fmm_param->nexpmax * 6 * (type == 'S')); 

  for (int i = 0; i < 8; i++) {
    if (subparts[i] > 0) {
      box->child[i] = hpx_gas_alloc(1, sizeof(fmm_box_t) + expansion_size); 
      set_box_action_arg_t cbox = {
	.type = type, 
	.addr = box->addr + addrs[i], 
	.npts = subparts[i], 
	.level = box->level + 1, 
	.parent = curr, 
	.index[0] = box->index[0] * 2 + xoff[i], 
	.index[1] = box->index[1] * 2 + yoff[i], 
	.index[2] = box->index[2] * 2 + zoff[i]
      }; 
      hpx_call(box->child[i], _set_box, &cbox, sizeof(set_box_action_arg_t), 
	       branch); 
    }
  }

  hpx_gas_unpin(curr); 
  hpx_lco_wait(branch); 
  hpx_lco_delete(branch, HPX_NULL); 
  return HPX_SUCCESS; 
}

int _swap_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  swap_action_arg_t *input = (swap_action_arg_t *) args; 

  char type = input->type; 
  int npts = input->npts; 
  int level = input->level; 
  int first = input->addr; 
  int last = first + npts; 
  double size = fmm_param->size; 
  double *corner = &fmm_param->corner[0]; 
  double h = size / (1 << (level + 1)); 
  double xc = corner[0] + (2 * input->index[0] + 1) * h; 
  double yc = corner[1] + (2 * input->index[1] + 1) * h; 
  double zc = corner[2] + (2 * input->index[2] + 1) * h;
  int *record = calloc(npts, sizeof(int)); 
  int result[16] = {0}, assigned[8] = {0}; 
  int *subparts = &result[0], *addrs = &result[8]; 

  if (type == 'S') {
    source_t *sources_p = NULL; 
    hpx_gas_try_pin(curr, (void *)&sources_p); 

    for (int i = first; i < last; i++) {
      double x = sources_p[i].position[0]; 
      double y = sources_p[i].position[1]; 
      double z = sources_p[i].position[2]; 
      int bin = 4 * (z > zc) + 2 * (y > yc) + (x > xc); 
      record[i - first] = bin; 
    } 

    for (int i = 0; i < npts; i++) 
      subparts[record[i]]++; 

    addrs[1] = addrs[0] + subparts[0]; 
    addrs[2] = addrs[1] + subparts[1]; 
    addrs[3] = addrs[2] + subparts[2]; 
    addrs[4] = addrs[3] + subparts[3]; 
    addrs[5] = addrs[4] + subparts[4];
    addrs[6] = addrs[5] + subparts[5]; 
    addrs[7] = addrs[6] + subparts[6]; 

    source_t *temp = calloc(npts, sizeof(source_t)); 
    for (int i = first; i < last; i++) {
      int bin = record[i - first]; 
      int offset = addrs[bin] + assigned[bin]++; 
      temp[offset] = sources_p[i]; 
    } 

    for (int i = first; i < last; i++) 
      sources_p[i] = temp[i - first]; 

    free(temp); 
  } else {
    target_t *targets_p = NULL; 
    hpx_gas_try_pin(curr, (void *)&targets_p); 

    for (int i = first; i < last; i++) {
      double x = targets_p[i].position[0]; 
      double y = targets_p[i].position[1]; 
      double z = targets_p[i].position[2]; 
      int bin = 4 * (z > zc) + 2 * (y > yc) + (x > xc); 
      record[i - first] = bin; 
    } 

    for (int i = 0; i < npts; i++) 
      subparts[record[i]]++; 

    addrs[1] = addrs[0] + subparts[0]; 
    addrs[2] = addrs[1] + subparts[1]; 
    addrs[3] = addrs[2] + subparts[2]; 
    addrs[4] = addrs[3] + subparts[3]; 
    addrs[5] = addrs[4] + subparts[4];
    addrs[6] = addrs[5] + subparts[5]; 
    addrs[7] = addrs[6] + subparts[6]; 

    target_t *temp = calloc(npts, sizeof(target_t)); 
    for (int i = first; i < last; i++) {
      int bin = record[i - first]; 
      int offset = addrs[bin] + assigned[bin]++; 
      temp[offset] = targets_p[i]; 
    } 

    for (int i = first; i < last; i++) 
      targets_p[i] = temp[i - first]; 

    free(temp); 
  }

  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(result); 
  return HPX_SUCCESS; 
} 

int _set_box_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *box = NULL; 
  hpx_gas_try_pin(curr, (void *)&box); 

  // Configure the new box
  set_box_action_arg_t *input = (set_box_action_arg_t *) args; 
  box->level = input->level; 
  box->parent = input->parent; 
  box->index[0] = input->index[0]; 
  box->index[1] = input->index[1]; 
  box->index[2] = input->index[2]; 
  box->npts = input->npts; 
  box->addr = input->addr; 

  char type = input->type; 
  if (type == 'S') 
    box->expan_avail = hpx_lco_and_new(3); 

  if (box->npts > s) {
    // Continue partitioning the box if it contains more than s points 
    hpx_addr_t status = hpx_lco_future_new(0); 
    hpx_call(curr, _partition_box, &type, sizeof(type), status); 
    hpx_lco_wait(status); 
    hpx_lco_delete(status, HPX_NULL); 
  } else {
    // Start the aggregate action at a leaf source box 
    if (type == 'S') 
      hpx_call(curr, _aggregate, NULL, 0, HPX_NULL); 
  }

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _aggregate_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&sbox); 

  int pgsz = fmm_param->pgsz; 
  bool last_arrival = false; 

  if (sbox->nchild == 0) {
    source_to_mpole_action_arg_t temp = {
      .addr = sbox->addr, 
      .npts = sbox->npts, 
      .level = sbox->level, 
      .index[0] = sbox->index[0], 
      .index[1] = sbox->index[1], 
      .index[2] = sbox->index[2]
    }; 

    hpx_addr_t result = hpx_lco_future_new(sizeof(double complex) * fmm_param->pgsz); 
    hpx_call(fmm_param->sources, _source_to_mpole, &temp, sizeof(temp), result); 
    hpx_lco_get(result, sizeof(double complex) * fmm_param->pgsz, &sbox->expansion[0]);
    hpx_lco_delete(result, HPX_NULL); 
  } else {
    double complex *input = (double complex *) args; 
    double complex *output = &sbox->expansion[0]; 
    hpx_lco_sema_p(sbox->sema); 
    for (int i = 0; i < pgsz; i++) 
      output[i] += input[i]; 
    last_arrival = (++sbox->n_reduce == sbox->nchild);
    hpx_lco_sema_v(sbox->sema);     
  } 

  if (sbox->nchild == 0 || last_arrival == true) {
    // Spawn tasks to translate multipole expansion into exponential expansions
    const char dir[3] = {'z', 'y', 'x'}; 
    hpx_call(curr, _mpole_to_expo, &dir[0], sizeof(char), sbox->expan_avail); 
    hpx_call(curr, _mpole_to_expo, &dir[1], sizeof(char), sbox->expan_avail); 
    hpx_call(curr, _mpole_to_expo, &dir[2], sizeof(char), sbox->expan_avail); 

    // Spawn task to translate the multipole expansion to its parent box
    int ichild = (sbox->index[2] % 2) * 4 + (sbox->index[1] % 2) * 2 + 
      (sbox->index[0] % 2); 
    hpx_call(curr, _mpole_to_mpole, &ichild, sizeof(int), HPX_NULL); 
  } 

  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _source_to_multipole_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  source_t *sources_p = NULL; 
  hpx_gas_try_pin(curr, (void *)&sources_p); 

  source_to_mpole_action_arg_t *input = (source_to_mpole_action_arg_t *) args; 
  int first = input->addr; 
  int npts = input->npts; 
  int last = first + npts; 
  int level = input->level; 
  double size = fmm_param->size; 
  double h = size / (1 << (level + 1)); 
  double *corner = &fmm_param->corner[0]; 
  double center[3]; 
  center[0] = corner[0] + (2 * input->index[0] + 1) * h; 
  center[1] = corner[1] + (2 * input->index[1] + 1) * h; 
  center[2] = corner[2] + (2 * input->index[2] + 1) * h; 

  int pgsz = fmm_param->pgsz; 
  int pterms = fmm_param->pterms; 
  double *ytopc = fmm_param->ytopc; 
  double scale = fmm_param->scale[level]; 

  const double precision = 1e-14; 
  double *powers = calloc(pterms + 1, sizeof(double)); 
  double *p = calloc(pgsz, sizeof(double)); 
  double complex *ephi = calloc(pterms + 1, sizeof(double complex)); 
  double complex *multipole = calloc(pgsz, sizeof(double complex)); 

  for (int i = first; i < last; i++) {
    double rx = sources_p[i].position[0] - center[0]; 
    double ry = sources_p[i].position[1] - center[1]; 
    double rz = sources_p[i].position[2] - center[2]; 
    double proj = rx * rx + ry * ry; 
    double rr = proj + rz * rz; 
    proj = sqrt(proj); 
    double d = sqrt(rr); 
    double ctheta = (d <= precision ? 1.0 : rz / d); 
    ephi[0] = (proj <= precision * d ? 1.0 : (rx + _Complex_I * ry) / proj); 
    d *= scale; 
    powers[0] = 1.0; 

    for (int ell = 1; ell <= pterms; ell++) {
      powers[ell] = powers[ell - 1] * d; 
      ephi[ell] = ephi[ell - 1] * ephi[0]; 
    } 

    double charge = sources_p[i].charge; 
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

  HPX_THREAD_CONTINUE(multipole); 
  hpx_gas_unpin(curr); 
  free(powers); 
  free(p); 
  free(ephi); 
  free(multipole);   
  return HPX_SUCCESS; 
}

int _multipole_to_multipole_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox); 

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

  hpx_call(sbox->parent, _aggregate, mpolen, 
	   sizeof(double complex) * pgsz, HPX_NULL); 
  free(ephi); 
  free(powers); 
  free(mpolen); 
  free(marray); 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS; 
}

int _multipole_to_exponential_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void **)&sbox); 

  const char dir = *((char *) args); 
  int pgsz = fmm_param->pgsz; 
  int nexpmax = fmm_param->nexpmax; 
  double *rdminus = fmm_param->rdminus; 
  double *rdplus = fmm_param->rdplus; 

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

int _disaggregate_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *tbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&tbox); 

  int pgsz = fmm_param->pgsz; 
  if (tbox->level == 0) {
    disaggregate_action_arg_t *output = 
      calloc(1, sizeof(disaggregate_action_arg_t) + sizeof(double complex) * pgsz); 
    output->plist5[0] = fmm_param->source_root; 
    output->nplist5 = 1; 

    for (int i = 0; i < 8; i++) {
      if (!hpx_addr_eq(tbox->child[i], HPX_NULL)) {
	hpx_call(tbox->child[i], _disaggregate, output, 
		 sizeof(disaggregate_action_arg_t) + sizeof(double complex) * pgsz, 
		 HPX_NULL); 
      }
    }
  } else {
    disaggregate_action_arg_t *input = (disaggregate_action_arg_t *) args;    
    for (int i = 0; i < pgsz; i++) 
      tbox->expansion[i] += input->expansion[i]; 

    int nlist1 = 0; 
    int nlist5 = 0; 
    hpx_addr_t list5[27] = {HPX_NULL}; 
    hpx_addr_t list1[27] = {HPX_NULL}; 

    // Determine the content of list 5
    int nplist1 = input->nplist1; 
    int nplist5 = input->nplist5; 
    hpx_addr_t result[27]; 
    for (int i = 0; i < nplist5; i++) {
      hpx_addr_t entry = input->plist5[i]; 
      result[i] = hpx_lco_future_new(sizeof(hpx_addr_t) * 4); 
      hpx_call(entry, _build_list5, &tbox->index[0], sizeof(int) * 3, result[i]); 
    } 

    for (int i = 0; i < nplist5; i++) {
      hpx_addr_t temp[4]; 
      hpx_lco_get(result[i], sizeof(hpx_addr_t) * 4, temp); 
      for (int j = 0; j < 4; j++) {
	if (!hpx_addr_eq(temp[j], HPX_NULL)) 
	  list5[nlist5++] = temp[j]; 
      }
      hpx_lco_delete(result[i], HPX_NULL); 
    }

    // Determine the content of list 1
    build_list1_action_arg_t build_list1_arg = {
      .index[0] = tbox->index[0], 
      .index[1] = tbox->index[1], 
      .index[2] = tbox->index[2], 
      .level = tbox->level, 
      .box = curr
    }; 

    for (int i = 0; i < nplist1; i++) {
      hpx_addr_t entry = input->plist1[i]; 
      result[i] = hpx_lco_future_new(sizeof(hpx_addr_t)); 
      hpx_call(entry, _build_list1, &build_list1_arg, sizeof(build_list1_action_arg_t),
	       result[i]); 
    }

    for (int i = 0; i < nplist1; i++) {
      hpx_addr_t temp; 
      hpx_lco_get(result[i], sizeof(hpx_addr_t), &temp); 
      if (hpx_addr_eq(temp, HPX_NULL)) 
	list1[nlist1++] = input->plist1[i]; 
    }

    // Check if the subtree rooted at the current target box can be pruned. 
    if (tbox->nchild) {
      // Check if the box is adjacent to any source box
      if (nlist5 == 0) {
	for (int i = 0; i < 8; i++) {
	  if (!hpx_addr_eq(tbox->child[i], HPX_NULL))
	    hpx_call(tbox->child[i], _delete_box, NULL, 0, HPX_NULL); 
	  tbox->child[i] = HPX_NULL; 
	}
	tbox->nchild = 0; 
      } else {
	// Check if any list5 has more than s points 
	bool remove = true; 
	hpx_addr_t query[27] = {HPX_NULL};
	for (int i = 0; i < nlist5; i++) {
	  query[i] = hpx_lco_future_new(sizeof(bool)); 
	  hpx_call(list5[i], _query_box, NULL, 0, query[i]); 
	}

	for (int i = 0; i < nlist5; i++) {
	  bool temp; 
	  hpx_lco_get(query[i], sizeof(bool), &temp); 
	  remove &= temp; 
	}

	if (remove) {
	  for (int i = 0; i < 8; i++) {
	    if (!hpx_addr_eq(tbox->child[i], HPX_NULL))
	      hpx_call(tbox->child[i], _delete_box, NULL, 0, HPX_NULL);
	    tbox->child[i] = HPX_NULL;
	  }
	  tbox->nchild = 0;
	}
      }
    }

    // Complete the exponential-to-local translation using merge-and-shift
    // technique if the target box remains nonleaf 
    if (tbox->nchild) {
      merge_shift_action_arg_t temp = {
	.index[0] = tbox->index[0],
	.index[1] = tbox->index[1], 
	.index[2] = tbox->index[2], 
	.child[0] = tbox->child[0], 
	.child[1] = tbox->child[1], 
	.child[2] = tbox->child[2], 
	.child[3] = tbox->child[3], 
	.child[4] = tbox->child[4], 
	.child[5] = tbox->child[5], 
	.child[6] = tbox->child[6], 
	.child[7] = tbox->child[7]
      }; 

      for (int i = 0; i < nlist5; i++) 
	hpx_call(list5[i], _merge_shift, &temp, sizeof(temp), HPX_NULL);
    } 

    // Wait on source-to-local to complete
    for (int i = 0; i < nplist1; i++) {
      if (!hpx_addr_eq(result[i], HPX_NULL)) 
	hpx_lco_wait(result[i]); 
      hpx_lco_delete(result[i], HPX_NULL); 
    }

    if (tbox->nchild) {
      // Translate the local expansion to the child boxes and invoke
      // disaggregate actions on the child boxes 
    } else {
      // Evaluate local expansion 

      // Process list 1 & 3

    }
  }

  hpx_gas_unpin(curr); 

  return HPX_SUCCESS; 
}  

int _build_list5_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&sbox); 

  int *input = (int *) args; 
  int tx = input[0]; 
  int ty = input[1];
  int tz = input[2]; 
  hpx_addr_t result[4] = {HPX_NULL}; 
  int iter = 0; 

  for (int i = 0; i < 8; i++) {
    int sx = sbox->index[i] * 2 + xoff[i]; 
    int sy = sbox->index[i] * 2 + yoff[i]; 
    int sz = sbox->index[i] * 2 + zoff[i]; 
    if (fabs(tx - sx) <= 1 && fabs(ty - sy) <= 1 && fabs(tz - sz) <= 1)
      result[iter++] = sbox->child[i]; 
  }

  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(result);
  return HPX_SUCCESS; 
}

int _build_list1_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&sbox); 
  
  build_list1_action_arg_t *input = (build_list1_action_arg_t *) args; 
  int tx = input->index[0]; 
  int ty = input->index[1]; 
  int tz = input->index[2]; 
  int sx = sbox->index[0]; 
  int sy = sbox->index[1]; 
  int sz = sbox->index[2]; 
  hpx_addr_t result = HPX_NULL; 

  if (fabs(tx - sx) > 1 || fabs(ty - sy) > 1 || fabs(tz - sz) > 1) {
    // The source box is a list 4 entry, invoke source-to-local action 
    result = hpx_lco_future_new(0); 
    source_to_local_action_arg_t temp = {
      .addr = sbox->addr, 
      .npts = sbox->npts, 
      .index[0] = tx, 
      .index[1] = ty, 
      .index[2] = tz, 
      .level = input->level, 
      .box = input->box, 
      .done = result
    }; 
    hpx_addr_t sources = fmm_param->sources; 
    hpx_call(sources, _source_to_local, &temp, sizeof(temp), HPX_NULL);
  }

  hpx_gas_unpin(curr); 
  HPX_THREAD_CONTINUE(result); 
  return HPX_SUCCESS; 
}

int _source_to_local_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  source_t *sources_p = NULL; 
  hpx_gas_try_pin(curr, (void *)&sources_p);

  source_to_local_action_arg_t *input = (source_to_local_action_arg_t *) args; 
  int pgsz = fmm_param->pgsz; 
  int pterms = fmm_param->pterms; 
  double *ytopc = fmm_param->ytopc; 
  int first = input->addr; 
  int npts = input->npts; 
  int last = first + npts;
  double h = fmm_param->size / (1 << (input->level + 1)); 
  double *corner = &fmm_param->corner[0]; 
  double center[3]; 
  center[0] = corner[0] + (2 * input->index[0] + 1) * h; 
  center[1] = corner[1] + (2 * input->index[1] + 1) * h; 
  center[2] = corner[2] + (2 * input->index[2] + 1) * h; 
  double scale = fmm_param->scale[input->level]; 
  double *powers = calloc(pterms + 3, sizeof(double)); 
  double *p = calloc(pgsz, sizeof(double)); 
  double complex *ephi = calloc(pterms + 2, sizeof(double complex)); 
  merge_local_action_arg_t *output = calloc(1, sizeof(merge_local_action_arg_t) + 
					    sizeof(double complex) * pgsz); 
  double complex *local = &output->expansion[0]; 
  output->done = input->done; 

  const double precision = 1e-14; 
  for (int i = first; i < last; i++) {
    double rx = sources_p[i].position[0] - center[0]; 
    double ry = sources_p[i].position[1] - center[1]; 
    double rz = sources_p[i].position[2] - center[2]; 
    double proj = rx * rx + ry * ry; 
    double rr = proj + rz * rz; 
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 1.0 : rz / d);
    ephi[0] = (proj <= precision * d ? 1.0 : (rx - _Complex_I * ry) / proj); 
    d = 1.0/d;
    powers[0] = 1.0;
    powers[1] = d;
    d /= scale; 

    for (int ell = 2; ell <= pterms + 2; ell++) 
      powers[ell] = powers[ell - 1] * d;

    for (int ell = 1; ell <= pterms + 1; ell++) 
      ephi[ell] = ephi[ell - 1] * ephi[0]; 

    local[0] += sources_p[i].charge * powers[1]; 
    lgndr(pterms, ctheta, p); 

    for (int ell = 1; ell <= pterms; ell++) 
      local[ell] += sources_p[i].charge * p[ell] * powers[ell + 1]; 

    for (int m = 1; m <= pterms; m++) {
      int offset1 = m * (pterms + 1);
      int offset2 = m * (pterms + 2); 
      for (int ell = m; ell <= pterms; ell++) {
        int index1 = offset1 + ell; 
        int index2 = offset2 + ell;
        local[index1] += sources_p[i].charge * powers[ell + 1] * 
          ytopc[index2] * p[index1] * ephi[m - 1]; 
      }
    }
  }

  // Merge local expansion
  hpx_call(input->box, _merge_local, output, 
	   sizeof(merge_local_action_arg_t) + sizeof(double complex) * pgsz, 
	   HPX_NULL); 
  free(powers); 
  free(p); 
  free(ephi); 
  free(output); 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS; 
}

int _merge_local_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *tbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&tbox); 
  merge_local_action_arg_t *input = (merge_local_action_arg_t *) args; 
  int pgsz = fmm_param->pgsz; 
  hpx_lco_sema_p(tbox->sema); 
  for (int i = 0; i < pgsz; i++) 
    tbox->expansion[0] += input->expansion[0]; 
  hpx_lco_sema_v(tbox->sema); 
  hpx_lco_set(input->done, 0, NULL, HPX_NULL, HPX_NULL); 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS; 
}

int _delete_box_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *box = NULL; 
  hpx_gas_try_pin(curr, (void *)&box); 
  for (int i = 0; i < 8; i++) {
    if (!hpx_addr_eq(box->child[i], HPX_NULL))
      hpx_call(box->child[i], _delete_box, NULL, 0, HPX_NULL); 
  }
  hpx_lco_delete(box->sema, HPX_NULL); 
  hpx_lco_delete(box->expan_avail, HPX_NULL); 
  hpx_gas_unpin(curr); 
  hpx_gas_global_free(curr, HPX_NULL); 
  return HPX_SUCCESS;
}

int _query_box_action(void) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *box = NULL; 
  hpx_gas_try_pin(curr, (void *)&box); 
  bool result = (box->npts <= s); 
  HPX_THREAD_CONTINUE(result); 
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _merge_shift_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&sbox); 

  merge_shift_action_arg_t *input = (merge_shift_action_arg_t *)args; 
  int action_argsz = sizeof(expo_to_local_action_arg_t); 
  
  for (int i = 0; i < 8; i++) {
    if (!hpx_addr_eq(sbox->child[i], HPX_NULL))
      hpx_call(sbox->child[i], _expo_to_local, input, action_argsz, HPX_NULL); 
  }
  hpx_gas_unpin(curr); 
  return HPX_SUCCESS;
}

int _exponential_to_local_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target(); 
  fmm_box_t *sbox = NULL; 
  hpx_gas_try_pin(curr, (void *)&sbox); 

  expo_to_local_action_arg_t *input = (expo_to_local_action_arg_t *) args; 
  int dx = sbox->index[0] - input->index[0]; 
  int dy = sbox->index[1] - input->index[1]; 
  int dz = sbox->index[2] - input->index[2]; 

  if (dz == 3) { 
    // uall 
  } else if (dz == -2) { 
    // dall
  } else if (dy == 3) {
    // nall
  } else if (dy == -2) {
    // sall
  } else if (dx == 3) {
    // eall
  } else if (dx == -2) {
    // wall
  } 

  if (dy >= -1 && dy <= 2 && dx >= -1 && dx <= 2) {
    int caseID = dy * 4 + dx + 5; 
    if (dz == 2) { // u1234
      switch (caseID) {
      case 0: // s78, w6
	break;
      case 1: // s78
	break;
      case 2: // s78
	break;
      case 3: // s78, e5
	break;
      case 4: // w68
	break;
      case 7: // e57
	break;
      case 8: // w68
	break;
      case 11:// e57
	break;
      case 12: // n56, w8
	break;
      case 13: // n56
	break;
      case 14: // n56
	break;
      case 15: // n56, e7
	break;
      } 
    } else if (dz == 1 || dz == 0) {
      switch (caseID) {
      case 0: // s3478, w2, w6
	break;
      case 1: // s3478
	break;
      case 2: // s3478
	break;
      case 3: // s3478, e1, e5
	break;
      case 4: // w2468
	break;
      case 7: // e1357
	break;
      case 8: // w2468
	break;
      case 11:// e1357
	break;
      case 12: // n1256, w4, w8
	break;
      case 13: // n1256
	break;
      case 14: // n1256
	break;
      case 15: // n1256, e3, e7
	break;
      } 	
    } else if (dz == -1) { // d5678 
      switch (caseID) {
      case 0: // s34, w2
	break;
      case 1: // s34
	break;
      case 2: // s34
	break;
      case 3: // s34, e1
	break;
      case 4: // w24
	break;
      case 7: // e13
	break;
      case 8: // w24
	break;
      case 11: // e13
	break;
      case 12: // n12, w4
	break;
      case 13: // n12
	break;
      case 14: // n12
	break;
      case 15: // n12, e3
	break;
      }
    }
  } 

  hpx_gas_unpin(curr); 
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

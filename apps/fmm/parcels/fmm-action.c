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
  hpx_addr_t sources = hpx_gas_alloc(nsources * sizeof(source_t));
  hpx_addr_t targets = hpx_gas_alloc(ntargets * sizeof(target_t));

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
  hpx_addr_t source_root = hpx_gas_alloc(sizeof(fmm_box_t));
  hpx_addr_t target_root = hpx_gas_alloc(sizeof(fmm_box_t));
  hpx_call(source_root, _init_source_root, NULL, 0, roots_done);
  hpx_call(target_root, _init_target_root, NULL, 0, roots_done);
  hpx_lco_wait(roots_done);
  hpx_lco_delete(roots_done, HPX_NULL);

  hpx_addr_t fmm_done = hpx_lco_and_new(ntargets);

  // Construct FMM param on each locality
  hpx_addr_t params_done = hpx_lco_future_new(0);
  init_param_action_arg_t init_param_arg = {
    .sources = sources,
    .targets = targets,
    .source_root = source_root,
    .target_root = target_root,
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
/*  hpx_addr_t partition_done = hpx_lco_and_new(2);
  char type1 = 'S', type2 = 'T';
  hpx_call(source_root, _partition_box, &type1, sizeof(type1), partition_done);
  hpx_call(target_root, _partition_box, &type2, sizeof(type2), partition_done);
  hpx_lco_wait(partition_done);
  hpx_lco_delete(partition_done, HPX_NULL);

  // Spawn disaggregate action along the target tree
  hpx_call(target_root, _disaggregate, NULL, 0, HPX_NULL);

  // Wait for completion
  hpx_lco_wait(fmm_param->fmm_done);
*/
  // Cleanup
  hpx_gas_free(sources, HPX_NULL);
  hpx_gas_free(targets, HPX_NULL);

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
  fmm_param->xs = calloc(fmm_param->nexpmax * 3, sizeof(double complex));
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
  int pgsz = fmm_param->pgsz;
  int nexpmax = fmm_param->nexpmax;


  int expan_size = sizeof(double complex) * (pgsz + nexpmax * 6 * (type == 'S'));

  for (int i = 0; i < 8; i++) {
    if (subparts[i] > 0) {
      box->child[i] = hpx_gas_alloc(sizeof(fmm_box_t) + expan_size);
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
  int and_gate_size = (type == 'S' ? 3 : 2);
  box->expan_avail = hpx_lco_and_new(and_gate_size);

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

    hpx_addr_t result = hpx_lco_future_new(sizeof(double complex) * pgsz);
    hpx_call(fmm_param->sources, _source_to_mpole, &temp, sizeof(temp), result);
    hpx_lco_get(result, sizeof(double complex) * pgsz, &sbox->expansion[0]);
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
  int nexpmax = fmm_param->nexpmax;
  int argsz =  sizeof(disaggregate_action_arg_t);
  disaggregate_action_arg_t *input = (disaggregate_action_arg_t *) args;
  int nlist1 = 0, nlist5 = 0;
  hpx_addr_t list1[27] = {HPX_NULL}, list5[27] = {HPX_NULL};

  if (tbox->level == 0) {
    list5[0] = fmm_param->source_root;
    nlist5 = 1;
    disaggregate_action_arg_t *output = calloc(1, argsz);
    output->nlist5 = nlist5;
    output->list5[0] = list5[0];
    for (int i = 0; i < 8; i++) {
      if (tbox->child[i])
        hpx_call(tbox->child[i], _disaggregate, output, argsz, HPX_NULL);
    }
  } else {
    int nplist1 = input->nlist1;
    int nplist5 = input->nlist5;
    hpx_addr_t result1[27], result5[27];

    // Spawn threads to determine the contents of lists 1 and 5
    for (int i = 0; i < nplist5; i++) {
      hpx_addr_t entry = input->list5[i];
      result5[i] = hpx_lco_future_new(sizeof(build_list5_action_return_t) * 4);
      hpx_call(entry, _build_list5, &tbox->index[0], sizeof(int) * 3, result5[i]);
    }

    for (int i = 0; i < nplist1; i++) {
      hpx_addr_t entry = input->list1[i];
      result1[i] = hpx_lco_future_new(sizeof(int) * 5);
      hpx_call(entry, _query_box, NULL, 0, result1[i]);
    }

    // Get lists 1 and 5 results
    for (int i = 0; i < nplist5; i++) {
      build_list5_action_return_t temp[4];
      hpx_lco_get(result5[i], sizeof(build_list5_action_return_t) * 4, temp);
      for (int j = 0; j < 4; j++) {
    if (temp[j].box) {
      if (temp[j].type == 5) {
        list5[nlist5++] = temp[j].box;
      } else {
        list1[nlist1++] = temp[j].box;
      }
    }
      }
    }

    for (int i = 0; i < nplist1; i++) {
      int temp[5];
      hpx_lco_get(result1[i], sizeof(int) * 5, temp);
      int dx = tbox->index[0] - temp[0];
      int dy = tbox->index[1] - temp[1];
      int dz = tbox->index[2] - temp[2];

      if (fabs(dx) > 1 || fabs(dy) > 1 || fabs(dz) > 1) {
    // The source box in plist1 is a list 4 entry of tbox, invoke
    // source-to-local action
    hpx_lco_delete(result1[i], HPX_NULL);
    result1[i] = hpx_lco_future_new(sizeof(double complex) * pgsz);

    source_to_local_action_arg_t source_to_local_arg = {
      .addr = temp[3],
      .npts = temp[4],
      .index[0] = tbox->index[0],
      .index[1] = tbox->index[1],
      .index[2] = tbox->index[2],
      .level = tbox->level
    };

    hpx_call(fmm_param->sources, _source_to_local, &source_to_local_arg,
         sizeof(source_to_local_action_arg_t), result1[i]);
      } else {
    // The source box in plist1 is a list 1 entry of tbox
    hpx_lco_delete(result1[i], HPX_NULL);
    result1[i] = HPX_NULL;
    list1[nlist1++] = input->list1[i];
      }
    }

    if (tbox->nchild) {
      if (nlist5 == 0) {
    // Prune the branch below tbox
    char type = 'T';
    for (int i = 0; i < 8; i++) {
      if (tbox->child[i])
        hpx_call(tbox->child[i], _delete_box, &type, sizeof(char), HPX_NULL);
      tbox->child[i] = HPX_NULL;
    }
    tbox->nchild = 0;
      } else {
    // Complete exponential-to-local operation using merge-and-shift
    int and_gate_size[28] = {36, 16, 24, 8, 4, 4, 16, 4, 2, 2, 3, 3, 3, 3,
                 36, 16, 24, 8, 4, 4, 16, 4, 2, 2, 3, 3, 3, 3};
    for (int i = 0; i < 28; i++)
      tbox->and_gates[i] = hpx_lco_and_new(and_gate_size[i]);

    tbox->merge = calloc(1, sizeof(double complex) * nexpmax * 28);

    merge_expo_action_arg_t merge_expo_arg = {
      .index[0] = tbox->index[0],
      .index[1] = tbox->index[1],
      .index[2] = tbox->index[2],
      .box = curr
    };

    for (int i = 0; i < nlist5; i++)
      hpx_call(list5[i], _merge_expo, &merge_expo_arg,
           sizeof(merge_expo_action_arg_t), HPX_NULL);

    // Wait on merge operation to complete
    for (int i = 0; i < 28; i++)
      hpx_lco_wait(tbox->and_gates[i]);

    // Shift the merged exponentials to the child boxes
    hpx_call(curr, _shift_expo_c1, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c2, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c3, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c4, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c5, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c6, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c7, NULL, 0, HPX_NULL);
    hpx_call(curr, _shift_expo_c8, NULL, 0, HPX_NULL);
      }
    }

    // Wait for completion of the source-to-local operation
    double complex *srcloc = calloc(1, sizeof(double complex) * pgsz);
    for (int i = 0; i < nplist1; i++) {
      if (result1[i]) {
    hpx_lco_get(result1[i], sizeof(double complex) * pgsz, srcloc);
    hpx_lco_sema_p(tbox->sema);
    for (int j = 0; j < pgsz; j++)
      tbox->expansion[j] += srcloc[j];
    hpx_lco_sema_v(tbox->sema);
    hpx_lco_delete(result1[i], HPX_NULL);
      }
    }
    free(srcloc);

    hpx_lco_wait(tbox->expan_avail);

    if (tbox->nchild) {
      disaggregate_action_arg_t *output = calloc(1, argsz);
      output->nlist1 = nlist1;
      output->nlist5 = nlist5;
      for (int i = 0; i < nlist1; i++)
    output->list1[i] = list1[i];
      for (int i = 0; i < nlist5; i++)
    output->list5[i] = list5[i];
      for (int i = 0; i < 8; i++) {
    if (tbox->child[i]) {
      hpx_call(curr, _local_to_local, &i, sizeof(int), HPX_NULL);
      hpx_call(tbox->child[i], _disaggregate, output, argsz, HPX_NULL);
    }
      }
    } else {
      int first = tbox->addr;
      int last = first + tbox->npts;

      for (int i = first; i < last; i++) {
    proc_target_action_arg_t proc_target_arg = {
      .id = i,
      .index[0] = tbox->index[0],
      .index[1] = tbox->index[1],
      .index[2] = tbox->index[2],
      .level = tbox->level,
      .box = curr,
      .nlist5 = nlist5
    };
    for (int i = 0; i < nlist5; i++)
      proc_target_arg.list5[i] = list5[i];

    hpx_call(fmm_param->targets, _proc_target, &proc_target_arg,
         sizeof(proc_target_action_arg_t), HPX_NULL);
      }
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
  int iter = 0;
  build_list5_action_return_t result[4];
  hpx_addr_t query[4];
  for (int i = 0; i < 4; i++) {
    result[i].box = HPX_NULL;
    result[i].type = -1;
  }

  for (int i = 0; i < 8; i++) {
    if (sbox->child[i]) {
      int dx = sbox->index[i] * 2 + xoff[i] - input[0];
      int dy = sbox->index[i] * 2 + yoff[i] - input[1];
      int dz = sbox->index[i] * 2 + zoff[i] - input[2];
      if (fabs(dx) <= 1 && fabs(dy) <= 1 && fabs(dz) <= 1) {
    result[iter].box = sbox->child[i];
    query[iter] = hpx_lco_future_new(sizeof(int) * 5);
    hpx_call(sbox->child[i], _query_box, NULL, 0, query[iter++]);
      }
    }
  }

  for (int i = 0; i < iter; i++) {
    int temp[5];
    hpx_lco_get(query[i], sizeof(int) * 5, temp);
    result[i].type = (temp[4] > s ? 5 : 1);
  }
  hpx_gas_unpin(curr);
  HPX_THREAD_CONTINUE(result);
  return HPX_SUCCESS;
}

int _query_box_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);

  int output[5] = {sbox->index[0], sbox->index[1], sbox->index[2],
           sbox->addr, sbox->npts};
  hpx_gas_unpin(curr);
  HPX_THREAD_CONTINUE(output);
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
  double complex *local = calloc(1, sizeof(double complex) * pgsz);

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

  hpx_gas_unpin(curr);
  HPX_THREAD_CONTINUE(local);
  free(powers);
  free(p);
  free(ephi);
  free(local);
  return HPX_SUCCESS;
}

int _delete_box_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *box = NULL;
  hpx_gas_try_pin(curr, (void *)&box);
  for (int i = 0; i < 8; i++) {
    if (box->child[i])
      hpx_call(box->child[i], _delete_box, NULL, 0, HPX_NULL);
  }
  hpx_lco_delete(box->sema, HPX_NULL);
  hpx_lco_delete(box->expan_avail, HPX_NULL);

  char type = *((char *) args);

  if (box->nchild && type == 'T') {
    for (int i = 0; i < 28; i++)
      hpx_lco_delete(box->and_gates[i], HPX_NULL);
    free(box->merge);
  }

  hpx_gas_unpin(curr);
  hpx_gas_free(curr, HPX_NULL);
  return HPX_SUCCESS;
}

int _merge_exponential_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);
  merge_expo_action_arg_t *input = (merge_expo_action_arg_t *) args;

  // each box belongs to at most three different merged lists
  const int table[3][16][3] = {
    // table for dz = -1
    { {15, 18, 24}, {15, 18, -1}, {15, 18, -1}, {15, 18, 10},
      {15, 22, -1}, {15, -1, -1}, {15, -1, -1}, {15, 8, -1},
      {15, 22, -1}, {15, -1, -1}, {15, -1, -1}, {15, 8, -1},
      {15, 4, 25}, {15, 4, -1}, {15, 4, -1}, {15, 4, 11} },
    // table for dz = 0 and dz = 1
    { {17, 24, 26}, {17, -1, -1}, {17, -1, -1}, {17, 10, 12},
      {21, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {7, -1, -1},
      {21, -1, -1}, {-1, -1, -1}, {-1, -1, -1}, {7, -1, -1},
      {3, 25, 27}, {3, -1, -1}, {3, -1, -1}, {3, 11, 13} },
    // table for dz = 2
    { {1, 19, 26}, {1, 19, -1}, {1, 19, -1}, {1, 19, 12},
      {1, 23, -1}, {1, -1, -1}, {1, -1, -1}, {1, 9, -1},
      {1, 23, -1}, {1, -1, -1}, {1, -1, -1}, {1, 9, -1},
      {1, 5, 27}, {1, 5, -1}, {1, 5, -1}, {1, 5, 13} }
  };

  for (int i = 0; i < 8; i++) {
    int dx = sbox->index[0] * 2 + xoff[i] - input->index[0] * 2;
    int dy = sbox->index[1] * 2 + yoff[i] - input->index[1] * 2;
    int dz = sbox->index[2] * 2 + zoff[i] - input->index[2] * 2;
    int dest[3] = {-1};

    if (dz == 3) { // uall
      dest[0] = 0;
    } else if (dz == -2) { // dall
      dest[0] = 14;
    } else if (dy == 3) { // nall
      dest[0] = 2;
    } else if (dy == -2) { // sall
      dest[0] = 16;
    } else if (dx == 3) { // eall
      dest[0] = 6;
    } else if (dx == -2) { // wall
      dest[0] = 20;
    } else if (dy >= -1 && dy <= 2 && dx >= -1 && dx <= 2) {
      const int *result = &table[(dz + 1) / 2][dy * 4 + dx + 5][0];
      dest[0] = result[0];
      dest[1] = result[1];
      dest[2] = result[2];
    }

    for (int j = 0; j < 3; j++) {
      if (dest[j] != -1) {
    int label = dest[j];
    if (sbox->child[i] == HPX_NULL) {
      merge_update_action_arg_t *merge_update_arg =
        calloc(1, sizeof(merge_update_action_arg_t));
      merge_update_arg->label = label;
      merge_update_arg->size = 0;
      hpx_call(input->box, _merge_update, merge_update_arg,
           sizeof(merge_update_action_arg_t), HPX_NULL);
      free(merge_update_arg);
    } else {
      merge_expo_z_action_arg_t temp = {
        .label = label,
        .box = input->box
      };

      if (label <= 1) {
        temp.offx = dx;
        temp.offy = dy;
        hpx_call(sbox->child[i], _merge_expo_zp, &temp, sizeof(temp), HPX_NULL);
      } else if (label <= 5) {
        temp.offx = dz;
        temp.offy = dx;
        hpx_call(sbox->child[i], _merge_expo_zp, &temp, sizeof(temp), HPX_NULL);
      } else if (label <= 13) {
        temp.offx = -dz;
        temp.offy = dy;
        hpx_call(sbox->child[i], _merge_expo_zp, &temp, sizeof(temp), HPX_NULL);
      } else if (label <= 15) {
        temp.offx = dx;
        temp.offy = dy;
        hpx_call(sbox->child[i], _merge_expo_zm, &temp, sizeof(temp), HPX_NULL);
      } else if (label <= 19) {
        temp.offx = dz;
        temp.offy = dx;
        hpx_call(sbox->child[i], _merge_expo_zm, &temp, sizeof(temp), HPX_NULL);
      } else {
        temp.offx = -dz;
        temp.offy = dy;
        hpx_call(sbox->child[i], _merge_expo_zm, &temp, sizeof(temp), HPX_NULL);
      }
    }
      }
    }
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _merge_exponential_zp_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);

  merge_expo_z_action_arg_t *input = (merge_expo_z_action_arg_t *) args;
  int nexpo = fmm_param->nexptotp;
  int nexpmax = fmm_param->nexpmax;
  int pgsz = fmm_param->pgsz;
  double complex *xs = fmm_param->xs;
  double complex *ys = fmm_param->ys;
  double complex *expo_in;

  if (input->label <= 1) {
    expo_in = &sbox->expansion[pgsz];
  } else if (input->label <= 5) {
    expo_in = &sbox->expansion[pgsz + nexpmax];
  } else if (input->label <= 13) {
    expo_in = &sbox->expansion[pgsz + nexpmax * 2];
  } else if (input->label <= 15) {
    expo_in = &sbox->expansion[pgsz];
  } else if (input->label <= 19) {
    expo_in = &sbox->expansion[pgsz + nexpmax];
  } else {
    expo_in = &sbox->expansion[pgsz + nexpmax * 2];
  }

  hpx_lco_wait(sbox->expan_avail);

  merge_update_action_arg_t *merge_update_arg =
    calloc(1, sizeof(merge_update_action_arg_t) +
       sizeof(double complex) * nexpmax);
  double complex *expo_out = &merge_update_arg->expansion[0];

  int offx = input->offx;
  int offy = input->offy;

  for (int i = 0; i < nexpo; i++) {
    double complex zmul = 1;
    if (offx > 0) {
      zmul *= xs[3 * i + offx - 1];
    } else if (offx < 0) {
      zmul *= conj(xs[3 * i - offx + 1]);
    }

    if (offy > 0) {
      zmul *= ys[3 * i + offy - 1];
    } else if (offy < 0) {
      zmul *= conj(ys[3 * i - offy + 1]);
    }

    expo_out[i] += zmul * expo_in[i];
  }

  hpx_gas_unpin(curr);
  hpx_call(input->box, _merge_update, merge_update_arg,
       sizeof(merge_update_action_arg_t) + sizeof(double complex) * nexpmax,
       HPX_NULL);
  free(merge_update_arg);
  return HPX_SUCCESS;
}


int _merge_exponential_zm_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);

  merge_expo_z_action_arg_t *input = (merge_expo_z_action_arg_t *) args;
  int nexpo = fmm_param->nexptotp;
  int pgsz = fmm_param->pgsz;
  int nexpmax = fmm_param->nexpmax;
  double complex *xs = fmm_param->xs;
  double complex *ys = fmm_param->ys;
  double complex *expo_in;

  if (input->label <= 1) {
    expo_in = &sbox->expansion[pgsz];
  } else if (input->label <= 5) {
    expo_in = &sbox->expansion[pgsz + nexpmax];
  } else if (input->label <= 13) {
    expo_in = &sbox->expansion[pgsz + nexpmax * 2];
  } else if (input->label <= 15) {
    expo_in = &sbox->expansion[pgsz];
  } else if (input->label <= 19) {
    expo_in = &sbox->expansion[pgsz + nexpmax];
  } else {
    expo_in = &sbox->expansion[pgsz + nexpmax * 2];
  }

  hpx_lco_wait(sbox->expan_avail);

  merge_update_action_arg_t *merge_update_arg =
    calloc(1, sizeof(merge_update_action_arg_t) +
       sizeof(double complex) * nexpmax);
  double complex *expo_out = &merge_update_arg->expansion[0];

  int offx = input->offx;
  int offy = input->offy;

  for (int i = 0; i < nexpo; i++) {
    double complex zmul = 1;
    if (offx > 0) {
      zmul *= conj(xs[3 * i + offx - 1]);
    } else if (offx < 0) {
      zmul *= xs[3 * i - offx + 1];
    }

    if (offy > 0) {
      zmul *= conj(ys[3 * i + offy - 1]);
    } else if (offy < 0) {
      zmul *= ys[3 * i - offy + 1];
    }

    expo_out[i] += zmul * expo_in[i];
  }

  hpx_gas_unpin(curr);
  hpx_call(input->box, _merge_update, merge_update_arg,
       sizeof(merge_update_action_arg_t) + sizeof(double complex) * nexpmax,
       HPX_NULL);
  free(merge_update_arg);
  return HPX_SUCCESS;
}

int _merge_update_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  merge_update_action_arg_t *input = (merge_update_action_arg_t *) args;

  int label = input->label;
  int size = input->size;
  if (size) {
    int nexpmax = fmm_param->nexpmax;
    double complex *expo_in = &input->expansion[0];
    double complex *expo_out = &tbox->merge[nexpmax * label];

    hpx_lco_sema_p(tbox->sema);
    for (int i = 0; i < size; i++)
      expo_out[i] += expo_in[i];
    hpx_lco_sema_v(tbox->sema);
  }

  hpx_lco_and_set(tbox->and_gates[label], HPX_NULL);
  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c1_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[0]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      double complex *u1234 = &tbox->merge[nexpmax];
      temp[i] = (uall[i] * zs[i3 + 2] + u1234[i] * zs[i3 + 1]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      temp[i] = dall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      double complex *n1256 = &tbox->merge[nexpmax * 3];
      double complex *n12 = &tbox->merge[nexpmax * 4];
      temp[i] = (nall[i] * zs[i3 + 2] +
         (n1256[i] + n12[i]) * zs[i3 + 1]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      temp[i] = sall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      double complex *e1357 = &tbox->merge[nexpmax * 7];
      double complex *e13 = &tbox->merge[nexpmax * 8];
      double complex *e1 = &tbox->merge[nexpmax * 10];
      temp[i] = (eall[i] * zs[i3 + 2] +
           (e1357[i] + e13[i] + e1[i]) * zs[i3 + 1]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      temp[i] = wall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[0], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c2_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[1]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      double complex *u1234 = &tbox->merge[nexpmax];
      temp[i] = (uall[i] * zs[i3 + 2] + u1234[i] * zs[i3 + 1]) *
    conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      temp[i] = dall[i] * zs[i3 + 1] * xs[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      double complex *n1256 = &tbox->merge[nexpmax * 3];
      double complex *n12 = &tbox->merge[nexpmax * 4];
      temp[i] = (nall[i] * zs[i3 + 2] +
         (n1256[i] + n12[i]) * zs[i3 + 1]) * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      temp[i] = sall[i] * zs[i3 + 1] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      temp[i] = eall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      double complex *w2468 = &tbox->merge[nexpmax * 21];
      double complex *w24 = &tbox->merge[nexpmax * 22];
      double complex *w2 = &tbox->merge[nexpmax * 24];
      temp[i] = (wall[i] * zs[i3 + 2] +
         (w2468[i] + w24[i] + w2[i]) * zs[i3 + 1]) *scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[1], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);
    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }


  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c3_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[2]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      double complex *u1234 = &tbox->merge[nexpmax];
      temp[i] = (uall[i] * zs[i3 + 2] + u1234[i] * zs[i3 + 1]) *
    conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      temp[i] = dall[i] * zs[i3 + 1] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      temp[i] = nall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      double complex *s3478 = &tbox->merge[nexpmax * 17];
      double complex *s34 = &tbox->merge[nexpmax * 18];
      temp[i] = (sall[i] * zs[i3 + 2]  +
         (s3478[i] + s34[i]) * zs[i3 + 1]) * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      double complex *e1357 = &tbox->merge[nexpmax * 7];
      double complex *e13 = &tbox->merge[nexpmax * 8];
      double complex *e3 = &tbox->merge[nexpmax * 11];
      temp[i] = (eall[i] * zs[i3 + 2] +
         (e1357[i] + e13[i] + e3[i]) * zs[i3 + 1]) * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      temp[i] = wall[i] * zs[i3 + 1] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[2], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c4_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[3]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      double complex *u1234 = &tbox->merge[nexpmax];
      temp[i] = (uall[i] * zs[i3 + 2] + u1234[i] * zs[i3 + 1]) *
    conj(xs[i3]) * ys[i3] *scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      temp[i] = dall[i] * zs[i3 + 1] * xs[i3] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      temp[i] = nall[i] * zs[i3 + 1] * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      double complex *s3478 = &tbox->merge[nexpmax * 17];
      double complex *s34 = &tbox->merge[nexpmax * 18];
      temp[i] = (sall[i] * zs[i3 + 2]  +
         (s3478[i] + s34[i]) * zs[i3 + 1]) * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      temp[i] = eall[i] * zs[i3 + 1] * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      double complex *w2468 = &tbox->merge[nexpmax * 21];
      double complex *w24 = &tbox->merge[nexpmax * 22];
      double complex *w4 = &tbox->merge[nexpmax * 25];
      temp[i] = (wall[i] * zs[i3 + 2] +
         (w2468[i] + w24[i] + w4[i]) * zs[i3 + 1]) * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[3], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c5_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[4]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      temp[i] = uall[i] * zs[i3 + 1] * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      double complex *d5678 = &tbox->merge[nexpmax * 15];
      temp[i] = (dall[i] * zs[i3 + 2] + d5678[i] * zs[i3 + 1]) * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      double complex *n1256 = &tbox->merge[nexpmax * 3];
      double complex *n56 = &tbox->merge[nexpmax * 5];
      temp[i] = (nall[i] * zs[i3 + 2] +
         (n1256[i] + n56[i]) * zs[i3 + 1]) * conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      temp[i] = sall[i] * zs[i3 + 1] * xs[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      double complex *e1357 = &tbox->merge[nexpmax * 7];
      double complex *e57 = &tbox->merge[nexpmax * 9];
      double complex *e5 = &tbox->merge[nexpmax * 12];
      temp[i] = (eall[i] * zs[i3 + 2] +
         (e1357[i] + e57[i] + e5[i]) * zs[i3 + 1]) * xs[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      temp[i] = wall[i] * zs[i3 + 1] * conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[4], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c6_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[5]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      temp[i] = uall[i] * zs[i3 + 1] * conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      double complex *d5678 = &tbox->merge[nexpmax * 15];
      temp[i] = (dall[i] * zs[i3 + 2] + d5678[i] * zs[i3 + 1]) * xs[i3] * scale;
   }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      double complex *n1256 = &tbox->merge[nexpmax * 3];
      double complex *n56 = &tbox->merge[nexpmax * 5];
      temp[i] = (nall[i] * zs[i3 + 2] + (n1256[i] + n56[i]) * zs[i3 + 1]) *
    conj(xs[i3] * ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      temp[i] = sall[i] * zs[i3 + 1] * xs[i3] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      temp[i] = eall[i] * zs[i3 + 1] * xs[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      double complex *w2468 = &tbox->merge[nexpmax * 21];
      double complex *w68 = &tbox->merge[nexpmax * 23];
      double complex *w6 = &tbox->merge[nexpmax * 26];
      temp[i] = (wall[i] * zs[i3 + 2] +
         (w2468[i] + w68[i] + w6[i]) * zs[i3 + 1]) * conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[5], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c7_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[6]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      temp[i] = uall[i] * zs[i3 + 1] * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      double complex *d5678 = &tbox->merge[nexpmax * 15];
      temp[i] = (dall[i] * zs[i3 + 2] + d5678[i] * zs[i3 + 1]) * ys[i3] * scale;
   }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      temp[i] = nall[i] * zs[i3 + 1] * conj(xs[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      double complex *s3478 = &tbox->merge[nexpmax * 17];
      double complex *s78 = &tbox->merge[nexpmax * 19];
      temp[i] = (sall[i] * zs[i3 + 2] + (s3478[i] + s78[i]) * zs[i3 + 1]) *
    xs[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      double complex *e1357 = &tbox->merge[nexpmax * 7];
      double complex *e57 = &tbox->merge[nexpmax * 9];
      double complex *e7 = &tbox->merge[nexpmax * 13];
      temp[i] = (eall[i] * zs[i3 + 2] + (e1357[i] + e57[i] + e7[i]) * zs[i3 + 1])
    * xs[i3] * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      temp[i] = wall[i] * zs[i3 + 1] + xs[i3] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[6], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _shift_exponential_c8_action(void) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);

  if (tbox->child[7]) {
    int nexpmax = fmm_param->nexpmax;
    int nexptotp = fmm_param->nexptotp;
    int pgsz = fmm_param->pgsz;
    int level = tbox->level;
    double scale = fmm_param->scale[level + 1];
    double complex *xs = fmm_param->xs;
    double complex *ys = fmm_param->ys;
    double *zs = fmm_param->zs;
    double *rdplus = fmm_param->rdplus;
    double *rdminus = fmm_param->rdminus;
    double complex *temp = calloc(1, sizeof(double complex) * nexpmax);
    double complex *local = calloc(1, sizeof(double complex) * pgsz);
    double complex *mexpf1 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mexpf2 = calloc(1, sizeof(double complex) * nexpmax);
    double complex *mw1 = calloc(1, sizeof(double complex) * pgsz);
    double complex *mw2 = calloc(1, sizeof(double complex) * pgsz);

    // +z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *uall = &tbox->merge[0];
      temp[i] = uall[i] * zs[i3 + 1] * conj(xs[i3] * ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -z direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *dall = &tbox->merge[nexpmax * 14];
      double complex *d5678 = &tbox->merge[nexpmax * 15];
      temp[i] = (dall[i] * zs[i3 + 2] + d5678[i] * zs[i3 + 1]) *
    xs[i3] * ys[i3] * scale;
   }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw1[i];

    // +y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *nall = &tbox->merge[nexpmax * 2];
      temp[i] = nall[i] * zs[i3 + 1] * conj(xs[i3] * ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -y direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *sall = &tbox->merge[nexpmax * 16];
      double complex *s3478 = &tbox->merge[nexpmax * 17];
      double complex *s78 = &tbox->merge[nexpmax * 19];
      temp[i] = (sall[i] * zs[i3 + 2] + (s3478[i] + s78[i]) * zs[i3 + 1]) *
    xs[i3] * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    roty2z(mw1, rdplus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    // +x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *eall = &tbox->merge[nexpmax * 6];
      temp[i] = eall[i] * zs[i3 + 1] * xs[i3] * conj(ys[i3]) * scale;
    }
    exponential_to_local_p1(temp, mexpf1);

    // -x direction
    for (int i = 0; i < nexptotp; i++) {
      int i3 = i * 3;
      double complex *wall = &tbox->merge[nexpmax * 20];
      double complex *w2468 = &tbox->merge[nexpmax * 21];
      double complex *w68 = &tbox->merge[nexpmax * 23];
      double complex *w8 = &tbox->merge[nexpmax * 27];
      temp[i] = (wall[i] * zs[i3 + 2] + (w2468[i] + w68[i] + w8[i]) * zs[i3 + 1])
    * conj(xs[i3]) * ys[i3] * scale;
    }
    exponential_to_local_p1(temp, mexpf2);

    exponential_to_local_p2(mexpf2, mexpf1, mw1);
    rotz2x(mw1, rdminus, mw2);
    for (int i = 0; i < pgsz; i++)
      local[i] += mw2[i];

    hpx_call(tbox->child[7], _merge_local, local,
         sizeof(double complex) * pgsz, HPX_NULL);

    free(temp);
    free(local);
    free(mexpf1);
    free(mexpf2);
    free(mw1);
    free(mw2);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

void exponential_to_local_p1(const double complex *mexpphys,
                             double complex *mexpf) {
  int nlambs = fmm_param->nlambs;
  int *numfour = fmm_param->numfour;
  int *numphys = fmm_param->numphys;
  double complex *fexpback = fmm_param->fexpback;

  int nftot = 0;
  int nptot = 0;
  int next  = 0;

  for (int i = 0; i < nlambs; i++) {
    int nalpha = numphys[i];
    int nalpha2 = nalpha / 2;
    mexpf[nftot] = 0;
    for (int ival = 0; ival < nalpha2; ival++) {
      mexpf[nftot] += 2.0 * creal(mexpphys[nptot + ival]);
    }
    mexpf[nftot] /= nalpha;

    for (int nm = 2; nm < numfour[i]; nm += 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
        double rtmp = 2 * creal(mexpphys[nptot + ival]);
        mexpf[nftot + nm] += fexpback[next] * rtmp;
        next++;
      }
      mexpf[nftot + nm] /= nalpha;
    }

    for (int nm = 1; nm < numfour[i]; nm += 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
        double complex ztmp = 2 * cimag(mexpphys[nptot + ival]) * _Complex_I;
        mexpf[nftot + nm] += fexpback[next] * ztmp;
        next++;
      }
      mexpf[nftot + nm] /= nalpha;
    }
    nftot += numfour[i];
    nptot += numphys[i] / 2;
  }
}

void exponential_to_local_p2(const double complex *mexpu,
                             const double complex *mexpd,
                             double complex *local) {
  int pterms = fmm_param->pterms;
  int nlambs = fmm_param->nlambs;
  int nexptot = fmm_param->nexptot;
  int pgsz = fmm_param->pgsz;
  double *whts = fmm_param->whts;
  double *rlams = fmm_param->rlams;
  int *numfour = fmm_param->numfour;
  double *ytopcs = fmm_param->ytopcs;

  double *rlampow = calloc(pterms + 1, sizeof(double));
  double complex *zeye = calloc(pterms + 1, sizeof(double complex));
  double complex *mexpplus = calloc(nexptot, sizeof(double complex));
  double complex *mexpminus = calloc(nexptot, sizeof(double complex));

  zeye[0] = 1.0;
  for (int i = 1; i <= pterms; i++)
    zeye[i] = zeye[i - 1] * _Complex_I;

  for (int i = 0; i < pgsz; i++)
    local[i] = 0;

  for (int i = 0; i < nexptot; i++) {
    mexpplus[i] = mexpd[i] + mexpu[i];
    mexpminus[i] = mexpd[i] - mexpu[i];
  }

  int ntot = 0;
  for (int nell = 0; nell < nlambs; nell++) {
    rlampow[0] = whts[nell];
    double rmul = rlams[nell];
    for (int j = 1; j <= pterms; j++)
      rlampow[j] = rlampow[j - 1] * rmul;

    int mmax = numfour[nell]-1;
    for (int mth = 0; mth <= mmax; mth += 2) {
      int offset = mth * (pterms + 1);
      for (int nm = mth; nm <= pterms; nm += 2) {
        int index = offset + nm;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul * mexpplus[ncurrent];
      }

      for (int nm = mth + 1; nm <= pterms; nm += 2) {
        int index = offset + nm;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul * mexpminus[ncurrent];
      }
    }

    for (int mth = 1; mth <= mmax; mth += 2) {
      int offset = mth * (pterms + 1);
      for (int nm = mth + 1; nm <= pterms; nm += 2) {
        int index = nm + offset;
        int ncurrent = ntot+mth;
        rmul = rlampow[nm];
        local[index] += rmul * mexpplus[ncurrent];
      }

      for (int nm = mth; nm <= pterms; nm += 2) {
        int index = nm + offset;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul * mexpminus[ncurrent];
      }
    }
    ntot += numfour[nell];
  }

  for (int mth = 0; mth <= pterms; mth++) {
    int offset1 = mth * (pterms + 1);
    int offset2 = mth * (pterms + 2);
    for (int nm = mth; nm <= pterms; nm++) {
      int index1 = nm + offset1;
      int index2 = nm + offset2;
      local[index1] *= zeye[mth] * ytopcs[index2];
    }
  }

  free(rlampow);
  free(zeye);
  free(mexpplus);
  free(mexpminus);
}

int _merge_local_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);
  double complex *input = (double complex *) args;
  int pgsz = fmm_param->pgsz;

  hpx_lco_sema_p(tbox->sema);
  for (int i = 0; i < pgsz; i++)
    tbox->expansion[i] += input[i];
  hpx_lco_sema_v(tbox->sema);
  hpx_lco_and_set(tbox->expan_avail, HPX_NULL);

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _local_to_local_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);
  int ichild = *((int *) args);
  const double complex var[5] =
    {1, 1 - _Complex_I, -1 - _Complex_I, -1 + _Complex_I, 1 + _Complex_I};
  const double arg = sqrt(2) / 2.0;
  const int ifld[8] = {1, 2, 4, 3, 1, 2, 4, 3};
  int pterms = fmm_param->pterms;
  int pgsz   = fmm_param->pgsz;
  double *dc = fmm_param->dc;
  double complex *localn = calloc(pgsz, sizeof(double complex));
  double complex *marray = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms, sizeof(double complex));
  double *powers = calloc(1 + pterms, sizeof(double));
  double complex *local = &tbox->expansion[0];

  double *rd = (ichild < 4 ? fmm_param->rdsq3 : fmm_param->rdmsq3);
  int ifl = ifld[ichild];
  ephi[0] = 1.0;
  ephi[1] = arg * var[ifl];
  double dd = -sqrt(3) / 4.0;
  powers[0] = 1.0;

  for (int ell = 1; ell <= pterms; ell++)
    powers[ell] = powers[ell - 1] * dd;

  for (int ell = 2; ell <= pterms; ell++)
    ephi[ell] = ephi[ell - 1] * ephi[1];

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      localn[index] = conj(ephi[m]) * local[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    int offset1 = (pterms + m) * pgsz;
    int offset2 = (pterms - m) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = localn[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp++) {
    int index1 = ell + mp * (pterms + 1);
    marray[index] += localn[index1] * rd[index1 + offset1] +
      conj(localn[index1]) * rd[index1 + offset2];
      }
    }
  }

  for (int k = 0; k <= pterms; k++) {
    int offset = k * (pterms + 1);
    for (int j = k; j <= pterms; j++) {
      int index = j + offset;
      localn[index] = marray[index];
      for (int ell = 1; ell <= pterms - j; ell++) {
    int index1 = ell + index;
    int index2 = ell + j + k + ell * (2 * pterms + 1);
    int index3 = ell + j - k + ell * (2 * pterms + 1);
    localn[index] += marray[index1] * powers[ell] *
      dc[index2] * dc[index3];
      }
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    int offset1 = (pterms + m) * pgsz;
    int offset2 = (pterms - m) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = localn[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
    int index1 = ell + mp * (pterms + 1);
    marray[index] -= localn[index1] * rd[index1 + offset1] +
      conj(localn[index1]) * rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
    int index1 = ell + mp * (pterms + 1);
    marray[index] += localn[index1] * rd[index1 + offset1] +
      conj(localn[index1]) * rd[index1 + offset2];
      }
    }
  }

  for (int m = 1; m <= pterms; m += 2) {
    int offset = m * (pterms + 1);
    int offset1 = (pterms + m) * pgsz;
    int offset2 = (pterms - m) * pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = -localn[ell] * rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
    int index1 = ell + mp * (pterms + 1);
    marray[index] += localn[index1] * rd[index1 + offset1] +
      conj(localn[index1]) * rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
    int index1 = ell + mp * (pterms + 1);
    marray[index] -= localn[index1] * rd[index1 + offset1] +
      conj(localn[index1]) * rd[index1 + offset2];
      }
    }
  }

  powers[0] = 1.0;
  for (int ell = 1; ell <= pterms; ell++)
    powers[ell] = powers[ell - 1] / 2;

  for (int m = 0; m <= pterms; m++) {
    int offset = m * (pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      localn[index] = ephi[m] * marray[index] * powers[ell];
    }
  }

  hpx_call(tbox->child[ichild], _merge_local, localn,
       sizeof(double complex) * pgsz, HPX_NULL);
  free(ephi);
  free(powers);
  free(localn);
  free(marray);
  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _proc_target_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  target_t *targets_p = NULL;
  hpx_gas_try_pin(curr, (void *)&targets_p);
  proc_target_action_arg_t *input = (proc_target_action_arg_t *) args;

  int id = input->id;
  int nlist1 = input->nlist1;
  int nlist5 = input->nlist5;
  double position[3];
  position[0] = targets_p[id].position[0];
  position[1] = targets_p[id].position[1];
  position[2] = targets_p[id].position[2];

  hpx_addr_t far_field_result = hpx_lco_future_new(sizeof(double) * 4);
  hpx_addr_t list1_result = hpx_lco_future_new(sizeof(double) * 4);
  hpx_addr_t list5_result = hpx_lco_future_new(sizeof(double) * 4);

  hpx_call(input->box, _local_to_target, position, sizeof(double) * 3,
       far_field_result);

  if (nlist1) {
    proc_list1_action_arg_t proc_list1_arg = {
      .position[0] = position[0],
      .position[1] = position[1],
      .position[2] = position[2],
      .potential = 0,
      .field[0] = 0,
      .field[1] = 0,
      .field[2] = 0,
      .nlist1 = nlist1,
      .curr = 0,
      .result = list1_result
    };

    for (int i = 0; i < nlist1; i++)
      proc_list1_arg.list1[i] = input->list1[i];

    hpx_call(input->list1[0], _proc_list1, &proc_list1_arg,
         sizeof(proc_list1_action_arg_t), HPX_NULL);
  }

  if (nlist5) {
    proc_list5_action_arg_t proc_list5_arg = {
      .level = input->level,
      .index[0] = input->index[0],
      .index[1] = input->index[1],
      .index[2] = input->index[2],
      .position[0] = position[0],
      .position[1] = position[1],
      .position[2] = position[2],
      .potential = 0,
      .field[0] = 0,
      .field[1] = 0,
      .field[2] = 0,
      .nlist5 = nlist5,
      .curr = 0,
      .result = list5_result
    };

    for (int i = 0; i < nlist5; i++)
      proc_list5_arg.list5[i] = input->list5[i];

    hpx_call(input->list5[0], _proc_list5, &proc_list5_arg,
         sizeof(proc_list5_action_arg_t), HPX_NULL);
  }

  double far_field_contrib[4] = {0};
  double list1_contrib[4] = {0};
  double list5_contrib[4] = {0};

  hpx_lco_get(far_field_result, sizeof(double) * 4, far_field_contrib);
  if (nlist1)
    hpx_lco_get(list1_result, sizeof(double) * 4, list1_contrib);
  if (nlist5)
    hpx_lco_get(list5_result, sizeof(double) * 4, list5_contrib);

  targets_p[id].potential = far_field_contrib[0] + list1_contrib[0] + list5_contrib[0];
  targets_p[id].field[0] = far_field_contrib[1] + list1_contrib[1] + list5_contrib[1];
  targets_p[id].field[1] = far_field_contrib[2] + list1_contrib[2] + list5_contrib[2];
  targets_p[id].field[2] = far_field_contrib[3] + list1_contrib[3] + list5_contrib[3];

  hpx_gas_unpin(curr);
  hpx_lco_and_set(fmm_param->fmm_done, HPX_NULL);
  return HPX_SUCCESS;
}

int _local_to_target_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *tbox = NULL;
  hpx_gas_try_pin(curr, (void *)&tbox);
  double *position = (double *) args;
  double result[4] = {0};

  int pgsz = fmm_param->pgsz;
  int pterms = fmm_param->pterms;
  double *ytopc = fmm_param->ytopc;
  double *ytopcs = fmm_param->ytopcs;
  double *ytopcsinv = fmm_param->ytopcsinv;
  double *p = calloc(pgsz, sizeof(double));
  double *powers = calloc(pterms + 1, sizeof(double));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));
  double complex *local = &tbox->expansion[0];
  int level = tbox->level;
  double h = fmm_param->size / (1 << (level + 1));
  double *corner = &fmm_param->corner[0];
  double center[3];
  center[0] = corner[0] + (2 * tbox->index[0] + 1) * h;
  center[1] = corner[1] + (2 * tbox->index[1] + 1) * h;
  center[2] = corner[2] + (2 * tbox->index[2] + 1) * h;
  double scale = fmm_param->scale[level];
  const double precision = 1e-14;

  double field0 = 0, field1 = 0, field2 = 0, rloc = 0, cp = 0, rpotz = 0;
  double complex cpz = 0, zs1 = 0, zs2 = 0, zs3 = 0;
  double rx = position[0] - center[0];
  double ry = position[1] - center[1];
  double rz = position[2] - center[2];
  double proj = rx * rx + ry * ry;
  double rr = proj + rz * rz;
  proj = sqrt(proj);
  double d = sqrt(rr);
  double ctheta = (d <= precision ? 0.0 : rz / d);
  ephi[0] = (proj <= precision * d ? 1.0 : (rx + _Complex_I * ry) / proj);
  d *= scale;
  double dd = d;

  powers[0] = 1.0;
  for (int ell = 1; ell <= pterms; ell++) {
    powers[ell] = dd;
    dd *= d;
    ephi[ell] = ephi[ell - 1] * ephi[0];
  }

  lgndr(pterms, ctheta, p);
  result[0] += creal(local[0]);

  field2 = 0.0;
  for (int ell = 1; ell <= pterms; ell++) {
    rloc = creal(local[ell]);
    cp = rloc * powers[ell] * p[ell];
    result[0] += cp;
    cp = powers[ell - 1] * p[ell - 1] * ytopcs[ell - 1];
    cpz = local[ell + pterms + 1] * cp * ytopcsinv[ell + pterms + 2];
    zs2 = zs2 + cpz;
    cp = rloc * cp * ytopcsinv[ell];
    field2 += cp;
  }

  for (int ell = 1; ell <= pterms; ell++) {
    for (int m = 1; m <= ell; m++) {
      int index = ell + m * (pterms + 1);
      cpz = local[index] * ephi[m - 1];
      rpotz += creal(cpz) * powers[ell] * ytopc[ell + m * (pterms + 2)] * p[index];
    }

    for (int m = 1; m <= ell - 1; m++) {
      int index1 = ell + m * (pterms + 1);
      int index2 = index1 - 1;
      zs3 += local[index1] * ephi[m - 1] * powers[ell - 1] *
    ytopc[ell - 1 + m * (pterms + 2)] * p[index2] *
    ytopcs[ell - 1 + m * (pterms + 2)] *
    ytopcsinv[ell + m * (pterms + 2)];
    }

    for (int m = 2; m <= ell; m++) {
      int index1 = ell + m * (pterms + 1);
      int index2 = ell - 1 + (m - 1) * (pterms + 1);
      zs2 += local[index1] * ephi[m - 2] *
    ytopcs[ell - 1 + (m - 1) * (pterms + 2)] *
    ytopcsinv[ell + m * (pterms + 2)] * powers[ell - 1] *
    ytopc[ell - 1 + (m - 1) * (pterms + 2)] * p[index2];
    }

    for (int m = 0; m <= ell - 2; m++) {
      int index1 = ell + m * (pterms + 1);
      int index2 = ell - 1 + (m + 1) * (pterms + 1);
      zs1 += local[index1] * ephi[m] *
    ytopcs[ell - 1 + (m + 1) * (pterms + 2)] *
    ytopcsinv[ell + m * (pterms + 2)] * powers[ell - 1] *
    ytopc[ell - 1 + (m + 1) * (pterms + 2)] * p[index2];

    }
  }

  result[0] += 2.0 * rpotz;
  field0 = creal(zs2 - zs1);
  field1 = -cimag(zs2 + zs1);
  field2 += 2.0 * creal(zs3);

  result[1] += field0 * scale;
  result[2] += field1 * scale;
  result[3] += field2 * scale;

  free(powers);
  free(ephi);
  free(p);

  hpx_gas_unpin(curr);
  HPX_THREAD_CONTINUE(result);
  return HPX_SUCCESS;
}

int _proc_list1_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);

  proc_list1_action_arg_t *param = (proc_list1_action_arg_t *) args;
  hpx_addr_t done = hpx_lco_future_new(sizeof(double) * 4);
  source_to_target_action_arg_t source_to_target_arg = {
    .addr = sbox->addr,
    .npts = sbox->npts,
    .position[0] = param->position[0],
    .position[1] = param->position[1],
    .position[2] = param->position[2]
  };

  hpx_call(fmm_param->sources, _source_to_target, &source_to_target_arg,
       sizeof(source_to_target_arg), done);
  double contrib[4];
  hpx_lco_get(done, sizeof(double) * 4, contrib);

  param->potential += contrib[0];
  param->field[0] += contrib[1];
  param->field[1] += contrib[2];
  param->field[2] += contrib[3];
  param->curr++;

  if (param->curr == param->nlist1) {
    double temp[4];
    temp[0] = param->potential;
    temp[1] = param->field[0];
    temp[2] = param->field[1];
    temp[3] = param->field[2];
    hpx_lco_set(param->result, sizeof(double) * 4, temp, HPX_NULL, HPX_NULL);
  } else {
    hpx_call(param->list1[param->curr], _proc_list1, param,
         sizeof(proc_list1_action_arg_t), HPX_NULL);
  }
  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _proc_list5_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);

  proc_list5_action_arg_t *param = (proc_list5_action_arg_t *)args;
  int dim = 1 << (param->level - sbox->level);
  bool is_adjacent =
    ((param->index[0] >= dim * sbox->index[0] - 1) &&
     (param->index[0] <= dim * sbox->index[0] + dim) &&
     (param->index[1] >= dim * sbox->index[1] - 1) &&
     (param->index[1] <= dim * sbox->index[1] + dim) &&
     (param->index[2] >= dim * sbox->index[2] - 1) &&
     (param->index[2] <= dim * sbox->index[2] + dim));

  double contrib[4] = {0};

  if (is_adjacent) {
    if (sbox->nchild) {
      int counter = 0;
      hpx_addr_t done[8];
      for (int i = 0; i < 8; i++) {
    if (sbox->child[i]) {
      done[counter] = hpx_lco_future_new(sizeof(double) * 4);
      hpx_call(sbox->child[i], _proc_list5, param,
           sizeof(proc_list5_action_arg_t), done[counter]);
      counter++;
    }
      }

      for (int i = 0; i < sbox->nchild; i++) {
    double temp[4] = {0};
    hpx_lco_get(done[i], sizeof(double) * 4, temp);
    contrib[0] += temp[0];
    contrib[1] += temp[1];
    contrib[2] += temp[2];
    contrib[3] += temp[3];
      }
    } else {
      hpx_addr_t done = hpx_lco_future_new(sizeof(double) * 4);
      source_to_target_action_arg_t source_to_target_arg = {
    .addr = sbox->addr,
    .npts = sbox->npts,
    .position[0] = param->position[0],
    .position[1] = param->position[1],
    .position[2] = param->position[2]
      };

      hpx_call(fmm_param->sources, _source_to_target, &source_to_target_arg,
           sizeof(source_to_target_action_arg_t), done);
      hpx_lco_get(done, sizeof(double) * 4, contrib);
    }
  } else {
    hpx_addr_t done = hpx_lco_future_new(sizeof(double) * 4);

    if (sbox->npts > fmm_param->pgsz) {
      // use multipole-to-target to process this list 3 box
    } else {
      // use source-to-target action
      source_to_target_action_arg_t source_to_target_arg = {
    .addr = sbox->addr,
    .npts = sbox->npts,
    .position[0] = param->position[0],
    .position[1] = param->position[1],
    .position[2] = param->position[2]
      };

      hpx_call(fmm_param->sources, _source_to_target, &source_to_target_arg,
           sizeof(source_to_target_action_arg_t), done);
    }
    hpx_lco_get(done, sizeof(double) * 4, contrib);
  }

  if (curr == param->list5[param->curr]) {
    param->potential += contrib[0];
    param->field[0] += contrib[1];
    param->field[1] += contrib[2];
    param->field[2] += contrib[3];
    param->curr++;

    if (param->curr == param->nlist5) {
      double temp[4];
      temp[0] = param->potential;
      temp[1] = param->field[1];
      temp[2] = param->field[2];
      hpx_lco_set(param->result, sizeof(double) * 4, temp, HPX_NULL, HPX_NULL);
    } else {
      hpx_call(param->list5[param->curr], _proc_list5, param,
           sizeof(proc_list5_action_arg_t), HPX_NULL);
    }
  } else {
    HPX_THREAD_CONTINUE(contrib);
  }

  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _source_to_target_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  source_t *sources_p = NULL;
  hpx_gas_try_pin(curr, (void *)&sources_p);

  source_to_target_action_arg_t *input = (source_to_target_action_arg_t *)args;
  int first = input->addr;
  int last = first + input->npts;
  double tx = input->position[0];
  double ty = input->position[1];
  double tz = input->position[2];
  double result[4] = {0};

  for (int i = first; i < last; i++) {
    double rx = tx - sources_p[i].position[0];
    double ry = ty - sources_p[i].position[1];
    double rz = tz - sources_p[i].position[2];
    double q = sources_p[i].charge;
    double rr = rx * rx + ry * ry + rz * rz;
    double rdis = sqrt(rr);

    if (rr) {
      result[0] += q / rdis;
      double rmul = q / (rdis * rr);
      result[1] += rmul * rx;
      result[2] += rmul * ry;
      result[3] += rmul * rz;
    }
  }

  HPX_THREAD_CONTINUE(result);
  hpx_gas_unpin(curr);
  return HPX_SUCCESS;
}

int _multipole_to_target_action(void *args) {
  hpx_addr_t curr = hpx_thread_current_target();
  fmm_box_t *sbox = NULL;
  hpx_gas_try_pin(curr, (void *)&sbox);
  double *input = (double *) args;

  int level = sbox->level;
  // int pgsz      = fmm_param->pgsz;
  int pterms    = fmm_param->pterms;
  double *ytopc = fmm_param->ytopc;
  double *ytopcs = fmm_param->ytopcs;
  double *ytopcsinv = fmm_param->ytopcsinv;
  double scale = fmm_param->scale[level];
  double complex *multipole = &sbox->expansion[0];
  double h = fmm_param->size / (1 << (level + 1));
  double *corner = &fmm_param->corner[0];
  double center[3];
  center[0] = corner[0] + (2 * sbox->index[0] + 1) * h;
  center[1] = corner[1] + (2 * sbox->index[1] + 1) * h;
  center[2] = corner[2] + (2 * sbox->index[2] + 1) * h;

  double *p = calloc((pterms + 2) * (pterms + 2), sizeof(double));
  double *powers = calloc(pterms + 4, sizeof(double));
  double complex *ephi = calloc(pterms + 3, sizeof(double complex));
  const double precis = 1e-14;

  double tx = input[0];
  double ty = input[1];
  double tz = input[2];
  double result[4] = {0};

  double rpotz = 0.0, field1 = 0.0, field2 = 0.0, field3 = 0.0;
  double complex zs1 = 0.0, zs2 = 0.0, zs3 = 0.0;
  double rx = tx - center[0];
  double ry = ty - center[1];
  double rz = tz - center[2];
  double proj = rx * rx + ry * ry;
  double rr = proj + rz * rz;
  proj = sqrt(proj);
  double d = sqrt(rr);
  double ctheta = (d <= precis ? 0.0 : rz / d);
  ephi[0] = (proj <= precis * d ? 1.0 : (rx + _Complex_I * ry) / proj);
  d = 1.0 / d;
  powers[0] = 1.0;
  powers[1] = d;
  d /= scale;

  for (int ell = 1; ell <= pterms + 2; ell++) {
    powers[ell + 1] = powers[ell] * d;
    ephi[ell] = ephi[ell - 1] * ephi[0];
  }

  lgndr(pterms + 1, ctheta, p);

  double rtemp;
  double rmp = creal(multipole[0]);
  result[0] += rmp * powers[1];
  double complex cpz = ephi[0] * rmp * powers[2] * ytopc[1 + pterms + 2] *
    p[1 + pterms + 2] * ytopcsinv[1 + pterms + 2];
  zs1 += cpz;
  double cp = rmp * powers[2] * p[1] * ytopcs[0] * ytopcsinv[1];
  field3 = cp;

  for (int ell = 1; ell <= pterms; ell++) {
    rmp = creal(multipole[ell]);
    cp = rmp * powers[ell + 1] * p[ell];
    result[0] += cp;
    zs1 += ephi[0] * rmp * powers[ell + 2] * ytopcs[ell + 1+ pterms + 2] *
      p[ell + 1 + pterms+2] * ytopcs[ell] * ytopcsinv[ell + 1 + pterms + 2];
    cpz = multipole[ell + pterms + 1];
    rtemp = powers[ell + 2] * p[ell + 1] * ytopcsinv[ell + 1];
    zs2 += cpz * rtemp * ytopcs[ell + pterms + 2];
    cp = rmp * rtemp * ytopcs[ell];
    field3 += cp;
  }

  for (int m = 1; m <= pterms; m++) {
    int offset1 = m * (pterms + 1);
    int offset2 = m * (pterms + 2);
    int offset5 = (m + 1) * (pterms + 2);
    int offset6 = (m - 1) * (pterms + 2);
    for (int ell = m; ell <= pterms; ell++) {
      int index1 = ell + offset1;
      int index2 = ell + offset2;
      int index5 = ell + 1 + offset5;
      int index6 = ell + 1 + offset6;
      cpz = multipole[index1] * powers[ell + 1] * ytopc[index2] * p[index2];
      rpotz += creal(cpz * ephi[m - 1]);
      cpz = multipole[index1] * ytopcs[index2] * powers[ell + 2];
      zs1 += cpz * ephi[m] * ytopcsinv[index5] * ytopc[index5] * p[index5];
      if (m > 1)
    zs2 += cpz * ephi[m - 2] * ytopcsinv[index6] *
      ytopc[index6] * p[index6];
      zs3 += cpz * ephi[m - 1] * ytopc[index2 + 1] * p[index2 + 1] *
    ytopcsinv[index2 + 1];
    }
  }

  result[0] += 2.0 * rpotz;
  field1 = creal(zs2 - zs1);
  field2 = -cimag(zs2 + zs1);
  field3 = field3 + 2.0 * creal(zs3);

  result[1] += field1 * scale;
  result[2] += field2 * scale;
  result[2] += field3 * scale;

  free(p);
  free(powers);
  free(ephi);

  HPX_THREAD_CONTINUE(result);
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


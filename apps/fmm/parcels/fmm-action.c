#include <stdio.h>
#include <stdlib.h>
#include <complex.h>
#include <math.h>
#include "hpx/hpx.h"
#include "fmm.h"

double *sources; 
double *charges; 
double *targets; 
double *potential; 
double *field; 

double *sources_pinned;
double *charges_pinned;
double *targets_pinned;
double *potential_pinned;
double *field_pinned;

fmm_dag_t *fmm_dag; 

hpx_addr_t far_field_syn; 
hpx_addr_t near_field_syn; 


int _fmm_main_action(void *args) {
  printf("generating random data: ");
  fflush(stdout);
  hpx_time_t fmm_exec_time = hpx_time_now();

  fmm_config_t *fmm_cfg = (fmm_config_t *)args;

  // generate test data according to the configuration
  int nsources = fmm_cfg->nsources;
  int ntargets = fmm_cfg->ntargets;
  int datatype = fmm_cfg->datatype;
  int s        = fmm_cfg->s;

  sources = calloc(nsources*3, sizeof(double));
  charges = calloc(nsources, sizeof(double));
  targets = calloc(ntargets*3, sizeof(double));
  potential = calloc(ntargets, sizeof(double));
  field = calloc(ntargets*3, sizeof(double));

  double pi = acos(-1);
  if (datatype == 1) {
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      charges[i] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3*i;
      targets[j] = 1.0*rand()/RAND_MAX - 0.5;
      targets[j + 1] = 1.0*rand()/RAND_MAX - 0.5;
      targets[j + 2] = 1.0*rand()/RAND_MAX - 0.5;
    }
  } else if (datatype == 2) {
    for (int i = 0; i < nsources; i++) {
      int j = 3*i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;

      charges[i] = 1.0*rand()/RAND_MAX - 0.5;
      sources[j] = sin(theta)*cos(phi);
      sources[j + 1] = sin(theta)*sin(phi);
      sources[j + 2] = cos(theta);
    }

    for (int i = 0; i < ntargets; i++) {
      int j = 3*i;
      double theta = 1.0*rand()/RAND_MAX*pi;
      double phi = 1.0*rand()/RAND_MAX*pi*2;
      targets[j] = sin(theta)*cos(phi);
      targets[j + 1] = sin(theta)*sin(phi);
      targets[j + 2] = cos(theta);
    }
  }

  double elapsed = hpx_time_elapsed_ms(fmm_exec_time);
  printf("%e seconds\n"
         "constructing dag: ", elapsed/1e3);
  fflush(stdout);
  fmm_exec_time = hpx_time_now();

  // generate fmm dag
  fmm_dag = construct_dag(sources, nsources, targets, ntargets, s);

  elapsed = hpx_time_elapsed_ms(fmm_exec_time);
  printf("%e seconds\n", elapsed/1e3);
  fflush(stdout);

  // display information of the dag
  int nsboxes = fmm_dag->nsboxes;
  int nslev   = fmm_dag->nslev;
  int ntboxes = fmm_dag->ntboxes;
  int ntlev   = fmm_dag->ntlev;
  double size = fmm_dag->size;

  printf("  source tree component: %3d levels, %d nodes\n"
         "  target tree component: %3d levels, %d nodes\n"
	 "rearranging input data: ",
         nslev, nsboxes, ntlev, ntboxes);
  fflush(stdout);

  fmm_exec_time = hpx_time_now();
  double *scale = calloc(1 + nslev, sizeof(double));
  scale[0] = 1/size;
  for (int i = 1; i <= nslev; i++)
    scale[i] = 2*scale[i - 1];

  hpx_addr_t sources_sorted = hpx_alloc(nsources*3*sizeof(double));
  hpx_addr_t charges_sorted = hpx_alloc(nsources*sizeof(double));
  hpx_addr_t targets_sorted = hpx_alloc(ntargets*3*sizeof(double));

  hpx_addr_try_pin(sources_sorted, (void **)&sources_pinned);
  hpx_addr_try_pin(charges_sorted, (void **)&charges_pinned);
  hpx_addr_try_pin(targets_sorted, (void **)&targets_pinned);

  int *mapsrc = fmm_dag->mapsrc;
  for (int i = 0; i < nsources; i++) {
    int j = mapsrc[i], i3 = i*3, j3 = j*3;
    sources_pinned[i3]     = sources[j3];
    sources_pinned[i3 + 1] = sources[j3 + 1];
    sources_pinned[i3 + 2] = sources[j3 + 2];
    charges_pinned[i]      = charges[j];
  }

  int *maptar = fmm_dag->maptar;
  for (int i = 0; i < ntargets; i++) {
    int j = maptar[i], i3 = i*3, j3 = j*3;
    targets_pinned[i3]     = targets[j3];
    targets_pinned[i3 + 1] = targets[j3 + 1];
    targets_pinned[i3 + 2] = targets[j3 + 2];
  }

  elapsed = hpx_time_elapsed_ms(fmm_exec_time);
  printf("%e seconds\n"
	 "allocating and initializing global data: ", elapsed/1e3);

  // allocate memory space to hold potential and field result
  hpx_addr_t potential_sorted = hpx_alloc(ntargets*sizeof(double));
  hpx_addr_t field_sorted     = hpx_alloc(ntargets*3*sizeof(double));
  hpx_addr_try_pin(potential_sorted, (void **)&potential_pinned);
  hpx_addr_try_pin(field_sorted, (void **)&field_pinned);

  for (int i = 0; i < ntargets; i++) {
    potential_pinned[i]   = 0;
    field_pinned[3*i]     = 0;
    field_pinned[3*i + 1] = 0;
    field_pinned[3*i + 2] = 0;
  }

  // allocate futures to hold expansions
  int pgsz    = fmm_param.pgsz;
  int nexpmax = fmm_param.nexpmax;
  {
    int block_size1 = sizeof(double complex)*pgsz; 
    int block_size2 = sizeof(double complex)*nexpmax; 
    mpole = hpx_lco_future_array_new(1 + nsboxes, block_size1, 1);
    local_h = hpx_lco_future_array_new(1 + ntboxes, block_size1, 1);
    local_v = hpx_lco_future_array_new(1 + ntboxes, block_size1, 1);
    expu = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
    expd = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
    expn = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
    exps = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
    expe = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
    expw = hpx_lco_future_array_new(1 + nsboxes, block_size2, 1);
  }

  {
    // broadcast the address of the futures to other localities
    fmm_expan_arg_t fmm_expan_arg = {
      .mpole   = mpole,
      .local_h = local_h,
      .local_v = local_v,
      .expu    = expu,
      .expd    = expd,
      .expn    = expn,
      .exps    = exps,
      .expe    = expe,
      .expw    = expw 
    };

    int num_ranks = hpx_get_num_ranks(); 
    hpx_addr_t bcast_syn = hpx_lco_future_array_new(num_ranks, 0, num_ranks); 

    for (int i = 1; i < num_ranks; i++) {
      hpx_call(HPX_THERE(i), _fmm_bcast, &fmm_expan_arg, 
	       sizeof(fmm_expan_arg), 
	       hpx_lco_future_array_at(bcast_syn, i)); 
    } 

    for (int i = 1; i < num_ranks; i++) 
      hpx_lco_wait(hpx_lco_future_array_at(bcast_syn, i)); 

    //hpx_lco_future_array_delete(bcast_syn);
  }


  // allocate futures for synchronization
  far_field_syn = hpx_lco_future_array_new(1 + ntboxes, 0, 1 + ntboxes); 
  near_field_syn = hpx_lco_future_array_new(1 + ntboxes, 0, 1 + ntboxes); 

  // set up the root box of the target tree
  hpx_lco_set(hpx_lco_future_array_at(far_field_syn, 1), NULL, 0, HPX_NULL);
  hpx_lco_set(hpx_lco_future_array_at(near_field_syn, 1), NULL, 0, HPX_NULL); 

  elapsed = hpx_time_elapsed_ms(fmm_exec_time);
  printf("%e seconds\n", elapsed/1e3);
  printf("fmm execution time: ");
  fflush(stdout);

  fmm_exec_time = hpx_time_now();

  // spawn threads to work on the source tree
  double corner_x = fmm_dag->corner[0];
  double corner_y = fmm_dag->corner[1];
  double corner_z = fmm_dag->corner[2];

  for (int i = nsboxes; i >= 1; i--) {
    fmm_box_t *sbox = fmm_dag->sboxptrs[i];
    if (sbox->nchild == 0) {
      int npts = sbox->npts;
      int addr = sbox->addr;

      hpx_parcel_t *p = hpx_parcel_acquire(sizeof(aggr_leaf_arg_t) + 
					   sizeof(double)*4*npts);       
      aggr_leaf_arg_t *aggr_leaf_arg = hpx_parcel_get_data(p); 

      aggr_leaf_arg->boxid = i;
      aggr_leaf_arg->nsources = npts;
      aggr_leaf_arg->scale = scale[sbox->level];

      double h = size/(1 << (sbox->level + 1));
      aggr_leaf_arg->center[0] = corner_x + (2*sbox->idx + 1)*h;
      aggr_leaf_arg->center[1] = corner_y + (2*sbox->idy + 1)*h;
      aggr_leaf_arg->center[2] = corner_z + (2*sbox->idz + 1)*h;

      for (int j = 0; j < npts; j++) {
        int j4 = j*4, k = addr + j, k3 = k*3;
        aggr_leaf_arg->points[j4]     = sources_pinned[k3];
        aggr_leaf_arg->points[j4 + 1] = sources_pinned[k3 + 1];
        aggr_leaf_arg->points[j4 + 2] = sources_pinned[k3 + 2];
        aggr_leaf_arg->points[j4 + 3] = charges_pinned[k];
      }

      hpx_parcel_set_action(p, _aggr_leaf_sbox); 
      hpx_parcel_set_target(p, hpx_lco_future_array_at(mpole, i)); 
      hpx_parcel_send(p); 
    } else {
      hpx_parcel_t *p = hpx_parcel_acquire(sizeof(aggr_nonleaf_arg_t)); 
      aggr_nonleaf_arg_t *aggr_nonleaf_arg = hpx_parcel_get_data(p); 

      aggr_nonleaf_arg->boxid = i; 
      aggr_nonleaf_arg->child[0] = sbox->child[0]; 
      aggr_nonleaf_arg->child[1] = sbox->child[1]; 
      aggr_nonleaf_arg->child[2] = sbox->child[2]; 
      aggr_nonleaf_arg->child[3] = sbox->child[3]; 
      aggr_nonleaf_arg->child[4] = sbox->child[4]; 
      aggr_nonleaf_arg->child[5] = sbox->child[5]; 
      aggr_nonleaf_arg->child[6] = sbox->child[6]; 
      aggr_nonleaf_arg->child[7] = sbox->child[7]; 

      hpx_parcel_set_action(p, _aggr_nonleaf_sbox); 
      hpx_parcel_set_target(p, hpx_lco_future_array_at(mpole, i)); 
      hpx_parcel_send(p); 
    }
  }

  // spawn threads to work on target box
  for (int i = 2; i <= ntboxes; i++) {
    // spawning thread for near-field work
    hpx_call(HPX_HERE, _process_near_field, &i, sizeof(i), 
	     hpx_lco_future_array_at(near_field_syn, i)); 

    // spawning thread for far-field work
    fmm_box_t *tbox = fmm_dag->tboxptrs[i];
    fmm_box_t *pbox = fmm_dag->tboxptrs[tbox->parent];

    if (tbox->nchild == 0) {
      int npts = tbox->npts;
      int addr = tbox->addr;

      hpx_parcel_t *p = hpx_parcel_acquire(sizeof(disaggr_leaf_arg_t) + 
					   sizeof(double)*3*npts); 
      disaggr_leaf_arg_t *disaggr_leaf_arg = hpx_parcel_get_data(p); 

      disaggr_leaf_arg->parent = tbox->parent;
      disaggr_leaf_arg->boxid = i;
      for (int j = 0; j < 8; j++) {
        if (i == pbox->child[j]) {
          disaggr_leaf_arg->which = j;
          break;
        }
      }

      disaggr_leaf_arg->ntargets = npts;
      disaggr_leaf_arg->scale = scale[tbox->level];

      double h = size/(1 << (tbox->level + 1));
      disaggr_leaf_arg->center[0] = corner_x + (2*tbox->idx + 1)*h;
      disaggr_leaf_arg->center[1] = corner_y + (2*tbox->idy + 1)*h;
      disaggr_leaf_arg->center[2] = corner_z + (2*tbox->idz + 1)*h;

      for (int j = 0; j < npts; j++) {
        int j3 = j*3, k = addr + j, k3 = k*3;
        disaggr_leaf_arg->points[j3]     = targets_pinned[k3];
        disaggr_leaf_arg->points[j3 + 1] = targets_pinned[k3 + 1];
        disaggr_leaf_arg->points[j3 + 2] = targets_pinned[k3 + 2];
      }

      hpx_parcel_set_action(p, _disaggr_leaf_tbox); 
      hpx_parcel_set_target(p, hpx_lco_future_array_at(local_h, i)); 
      hpx_parcel_send(p); 
    } else {
      hpx_parcel_t *p = hpx_parcel_acquire(sizeof(disaggr_nonleaf_arg_t)); 
      disaggr_nonleaf_arg_t *disaggr_nonleaf_arg = hpx_parcel_get_data(p); 

      disaggr_nonleaf_arg->parent = tbox->parent;
      disaggr_nonleaf_arg->boxid = i;
      for (int j = 0; j < 8; j++) {
        if (i == pbox->child[j]) {
          disaggr_nonleaf_arg->which = j;
          break;
        }
      }

      disaggr_nonleaf_arg->level = tbox->level;
      disaggr_nonleaf_arg->child[0] = tbox->child[0];
      disaggr_nonleaf_arg->child[1] = tbox->child[1];
      disaggr_nonleaf_arg->child[2] = tbox->child[2];
      disaggr_nonleaf_arg->child[3] = tbox->child[3];
      disaggr_nonleaf_arg->child[4] = tbox->child[4];
      disaggr_nonleaf_arg->child[5] = tbox->child[5];
      disaggr_nonleaf_arg->child[6] = tbox->child[6];
      disaggr_nonleaf_arg->child[7] = tbox->child[7];

      build_merged_list2(fmm_dag, disaggr_nonleaf_arg);
      disaggr_nonleaf_arg->scale = scale[tbox->level + 1];

      hpx_parcel_set_action(p, _disaggr_nonleaf_tbox); 
      hpx_parcel_set_target(p, hpx_lco_future_array_at(local_h, i));
      hpx_parcel_set_cont(p, hpx_lco_future_array_at(far_field_syn, i)); 
      hpx_parcel_send(p); 
    }
  }

  // wait for the work on the target tree to complete
  for (int i = 1; i <= ntboxes; i++) {
    hpx_lco_wait(hpx_lco_future_array_at(far_field_syn, i)); 
    hpx_lco_wait(hpx_lco_future_array_at(near_field_syn, i)); 
  }

  elapsed = hpx_time_elapsed_ms(fmm_exec_time);
  printf("%e seconds\n", elapsed/1e3);
  printf("fmm accuracy statistics:\n");
  fflush(stdout);

  // write potential and field result in the original input order
  for (int i = 0; i < ntargets; i++) {
    int j = maptar[i], i3 = i*3, j3 = j*3;
    potential[j]  += potential_pinned[i];
    field[j3]     += field_pinned[i3];
    field[j3 + 1] += field_pinned[i3 + 1];
    field[j3 + 2] += field_pinned[i3 + 2];
  }

  int n_verify = (ntargets < 200 ? ntargets : 200);
  double salg = 0, salg2 = 0, stot = 0, stot2 = 0, errmax = 0;

  for (int i = 0; i < n_verify; i++) {
    int i3 = i*3;
    const double *t = &targets[i3];
    double pot = 0, fx = 0, fy = 0, fz = 0;
    for (int j = 0; j < nsources; j++) {
      int j3 = j*3;
      const double *s = &sources[j3];
      const double q = charges[j];
      double rx = t[0] - s[0];
      double ry = t[1] - s[1];
      double rz = t[2] - s[2];
      double rr = rx*rx + ry*ry + rz*rz;
      double rdis = sqrt(rr);

      if (rr) {
        pot += q/rdis;
        double rmul = q/(rdis*rr);
        fx += rmul*rx;
        fy += rmul*ry;
        fz += rmul*rz;
      }
    }

    salg += (potential[i] - pot)*(potential[i] - pot);
    stot += pot*pot;
    salg2 += (field[i3] - fx)*(field[i3] - fx) +
             (field[i3 + 1] - fy)*(field[i3 + 1] - fy) +
             (field[i3 + 2] - fz)*(field[i3 + 2] - fz);
    stot2 += fx*fx + fy*fy + fz*fz;
    errmax = fmax(errmax, fabs(potential[i] - pot));
  }

  printf("  error of potential in L-2 norm: %e\n"
         "  error of potential in L-infty norm: %e\n"
         "  error of field in L-2 norm: %e\n",
         sqrt(salg/stot), errmax, sqrt(salg2/stot2));

  int if_pass = (sqrt(salg/stot) <= pow(10.0, -fmm_cfg->accuracy)) &&
                (sqrt(salg2/stot2) <= pow(10.0, -fmm_cfg->accuracy + 1));
  printf(if_pass ? "******* pass *******\n" : "******* fail *******\n");

  destruct_dag(fmm_dag);

  free(sources);
  free(charges);
  free(targets);
  free(potential);
  free(field);
  free(scale);
  //hpx_global_free(sources_sorted);
  //hpx_global_free(charges_sorted);
  //hpx_global_free(targets_sorted);
  //hpx_global_free(potentail_sorted);
  //hpx_global_free(field_sorted);
  //hpx_lco_future_array_delete(far_field_syn);
  //hpx_lco_future_array_delete(near_field_syn); 
  //hpx_lco_future_array_delete(mpole);
  //hpx_lco_future_array_delete(local);
  //hpx_lco_future_array_delete(expu);
  //hpx_lco_future_array_delete(expd);
  //hpx_lco_future_array_delete(expn);
  //hpx_lco_future_array_delete(exps);
  //hpx_lco_future_array_delete(expe);
  //hpx_lco_future_array_delete(expw);
  hpx_shutdown(0);
  return HPX_SUCCESS;
}


int _fmm_bcast_action(void *args) {
  // _fmm_bcast_action broadcast the address of the global future
  // array allocated on the root node. 
  fmm_expan_arg_t *fmm_expan_arg = (fmm_expan_arg_t *)args; 

  mpole   = fmm_expan_arg->mpole; 
  local_h = fmm_expan_arg->local_h;
  local_v = fmm_expan_arg->local_v; 
  expu    = fmm_expan_arg->expu; 
  expd    = fmm_expan_arg->expd; 
  expn    = fmm_expan_arg->expn;
  exps    = fmm_expan_arg->exps; 
  expe    = fmm_expan_arg->expe; 
  expw    = fmm_expan_arg->expw; 

  hpx_thread_continue(0, NULL); 
}


int _aggr_leaf_sbox_action(void *args) {
  // _aggr_leaf_sbox_action generates the multipole expansion of the
  // specified leaf box and six directional exponential expansions.
  aggr_leaf_arg_t *aggr_leaf_arg = (aggr_leaf_arg_t *)args;

  int boxid        = aggr_leaf_arg->boxid;
  int nsources     = aggr_leaf_arg->nsources;
  double scale     = aggr_leaf_arg->scale;
  double *center   = aggr_leaf_arg->center;
  double *points   = aggr_leaf_arg->points;

  hpx_addr_t my_mpole = hpx_lco_future_array_at(mpole, boxid);
  hpx_addr_t my_expu  = hpx_lco_future_array_at(expu, boxid);
  hpx_addr_t my_expd  = hpx_lco_future_array_at(expd, boxid);
  hpx_addr_t my_expn  = hpx_lco_future_array_at(expn, boxid);
  hpx_addr_t my_exps  = hpx_lco_future_array_at(exps, boxid);
  hpx_addr_t my_expe  = hpx_lco_future_array_at(expe, boxid);
  hpx_addr_t my_expw  = hpx_lco_future_array_at(expw, boxid);

  int pgsz = fmm_param.pgsz;
  int nexpmax = fmm_param.nexpmax;

  double complex *multipole = calloc(pgsz, sizeof(double complex));
  double complex *mexpf1 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf2 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf3 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf4 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf5 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf6 = calloc(nexpmax, sizeof(double complex));
  double complex *mw     = calloc(pgsz, sizeof(double complex));

  source_to_multipole(points, nsources, center, scale, multipole);

  multipole_to_exponential(multipole, mexpf1, mexpf2);

  rotz2y(multipole, fmm_param.rdminus, mw);
  multipole_to_exponential(mw, mexpf3, mexpf4);

  rotz2x(multipole, fmm_param.rdplus, mw);
  multipole_to_exponential(mw, mexpf5, mexpf6);

  hpx_lco_set(my_mpole, multipole, sizeof(double complex)*pgsz, HPX_NULL);
  hpx_lco_set(my_expu, mexpf1, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expd, mexpf2, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expn, mexpf3, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_exps, mexpf4, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expe, mexpf5, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expw, mexpf6, sizeof(double complex)*nexpmax, HPX_NULL);

  free(multipole);
  free(mexpf1);
  free(mexpf2);
  free(mexpf3);
  free(mexpf4);
  free(mexpf5);
  free(mexpf6);
  free(mw);

  return HPX_SUCCESS;
}


int _aggr_nonleaf_sbox_action(void *args) {
  // _aggr_nonleaf_sbox_action generates the multipole expansion of
  // the specified nonleaf box by translating the multipole expansions
  // of its child boxes. It also generates the six directional
  // exponential expansions.

  aggr_nonleaf_arg_t *aggr_nonleaf_arg = (aggr_nonleaf_arg_t *)args;

  int boxid        = aggr_nonleaf_arg->boxid;

  hpx_addr_t my_mpole = hpx_lco_future_array_at(mpole, boxid);
  hpx_addr_t my_expu  = hpx_lco_future_array_at(expu, boxid);
  hpx_addr_t my_expd  = hpx_lco_future_array_at(expd, boxid);
  hpx_addr_t my_expn  = hpx_lco_future_array_at(expn, boxid);
  hpx_addr_t my_exps  = hpx_lco_future_array_at(exps, boxid);
  hpx_addr_t my_expe  = hpx_lco_future_array_at(expe, boxid);
  hpx_addr_t my_expw  = hpx_lco_future_array_at(expw, boxid);

  int pgsz = fmm_param.pgsz;
  int nexpmax = fmm_param.nexpmax;

  double complex *multipole = calloc(pgsz, sizeof(double complex));
  double complex *temp      = calloc(pgsz, sizeof(double complex));

  for (int i = 0; i < 8; i++) {
    int child = aggr_nonleaf_arg->child[i];
    if (child) {
      hpx_addr_t child_mpole = hpx_lco_future_array_at(mpole, child);
      hpx_lco_get(child_mpole, temp, sizeof(double complex)*pgsz);
      multipole_to_multipole(temp, i, multipole);
    }
  }

  double complex *mexpf1 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf2 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf3 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf4 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf5 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf6 = calloc(nexpmax, sizeof(double complex));
  double complex *mw     = calloc(pgsz, sizeof(double complex));

  multipole_to_exponential(multipole, mexpf1, mexpf2);

  rotz2y(multipole, fmm_param.rdminus, mw);
  multipole_to_exponential(mw, mexpf3, mexpf4);

  rotz2x(multipole, fmm_param.rdplus, mw);
  multipole_to_exponential(mw, mexpf5, mexpf6);

  hpx_lco_set(my_mpole, multipole, sizeof(double complex)*pgsz, HPX_NULL);
  hpx_lco_set(my_expu, mexpf1, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expd, mexpf2, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expn, mexpf3, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_exps, mexpf4, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expe, mexpf5, sizeof(double complex)*nexpmax, HPX_NULL);
  hpx_lco_set(my_expw, mexpf6, sizeof(double complex)*nexpmax, HPX_NULL);

  free(multipole);
  free(temp);
  free(mw);
  free(mexpf1);
  free(mexpf2);
  free(mexpf3);
  free(mexpf4);
  free(mexpf5);
  free(mexpf6);

  return HPX_SUCCESS;
}


int _disaggr_leaf_tbox_action(void *args) {
  // _disaggr_leaf_tbox_action shifts the local expansion of the
  // parent of the specified box and then computes the expansion at
  // each target point contained in the box.
  disaggr_leaf_arg_t *disaggr_leaf_arg = (disaggr_leaf_arg_t *)args;

  int pgsz = fmm_param.pgsz;
  double complex *plocal   = calloc(pgsz, sizeof(double complex));
  double complex *mlocal_v = calloc(pgsz, sizeof(double complex));
  double complex *mlocal_h = calloc(pgsz, sizeof(double complex));

  int parent = disaggr_leaf_arg->parent;
  int boxid = disaggr_leaf_arg->boxid;
  int which = disaggr_leaf_arg->which;
  hpx_addr_t parent_local = hpx_lco_future_array_at(local_v, parent);
  hpx_lco_get(parent_local, plocal, sizeof(double complex)*pgsz);
  local_to_local(plocal, which, mlocal_v);

  hpx_addr_t my_local_h = hpx_lco_future_array_at(local_h, boxid);
  hpx_lco_get(my_local_h, mlocal_h, sizeof(double complex)*pgsz);

  for (int i = 0; i < pgsz; i++)
    mlocal_v[i] += mlocal_h[i];

  // create a parcel to send the potential and field result back to
  // the native thread
  double *points = disaggr_leaf_arg->points;
  double *center = disaggr_leaf_arg->center;
  double scale   = disaggr_leaf_arg->scale;
  int ntargets   = disaggr_leaf_arg->ntargets;

  hpx_parcel_t *p = hpx_parcel_acquire(sizeof(fmm_result_arg_t) + 
				       sizeof(double)*4*ntargets); 
  fmm_result_arg_t *fmm_result_arg = hpx_parcel_get_data(p); 
  fmm_result_arg->boxid = boxid; 
  local_to_target(mlocal_v, center, scale, points, ntargets, 
		  fmm_result_arg->result); 

  hpx_parcel_set_action(p, _recv_result); 
  hpx_parcel_set_target(p, HPX_THERE(0)); 
  hpx_parcel_send(p); 

  free(mlocal_v);
  free(mlocal_h);
  free(plocal);
  return HPX_SUCCESS;
}


int _recv_result_action(void *args) {
  // _recv_result_action processes the received potential and field
  // result  
  fmm_result_arg_t *fmm_result_arg = (fmm_result_arg_t *)args; 
  int boxid = fmm_result_arg->boxid; 
  double *result = fmm_result_arg->result; 
  
  fmm_box_t *tbox = fmm_dag->tboxptrs[boxid]; 
  int addr = tbox->addr; 
  int ntargets = tbox->npts; 

  for (int i = 0; i < ntargets; i++) {
    int j = addr + i, j3 = j*3, i4 = i*4;
    potential_pinned[j]  += result[i4];
    field_pinned[j3]     += result[i4 + 1];
    field_pinned[j3 + 1] += result[i4 + 2];
    field_pinned[j3 + 2] += result[i4 + 3];
  }

  hpx_lco_set(hpx_lco_future_array_at(far_field_syn, boxid), 
	      NULL, 0, HPX_NULL); 

  return HPX_SUCCESS;
}


int _disaggr_nonleaf_tbox_action(void *args) {
  // _disaggr_nonleaf_tbox_action shifts the local expansion of the
  // parent of the specified box. It also completes the
  // exponential-to-local translation for the child boxes using the
  // merge-and-shift technique.

  disaggr_nonleaf_arg_t *disaggr_nonleaf_arg = (disaggr_nonleaf_arg_t *)args;

  int pgsz = fmm_param.pgsz;
  int parent = disaggr_nonleaf_arg->parent;
  int boxid = disaggr_nonleaf_arg->boxid;
  int which = disaggr_nonleaf_arg->which;
  int level = disaggr_nonleaf_arg->level;

  // completes the exponential-to-local translation
  int child[8];
  child[0] = disaggr_nonleaf_arg->child[0];
  child[1] = disaggr_nonleaf_arg->child[1];
  child[2] = disaggr_nonleaf_arg->child[2];
  child[3] = disaggr_nonleaf_arg->child[3];
  child[4] = disaggr_nonleaf_arg->child[4];
  child[5] = disaggr_nonleaf_arg->child[5];
  child[6] = disaggr_nonleaf_arg->child[6];
  child[7] = disaggr_nonleaf_arg->child[7];

  double complex *clocal = calloc(pgsz*8, sizeof(double complex));
  exponential_to_local(disaggr_nonleaf_arg, child, clocal);

  for (int i = 0; i < 8; i++) {
    if (child[i]) {
      hpx_addr_t child_local_h = hpx_lco_future_array_at(local_h, child[i]);
      hpx_lco_set(child_local_h, &clocal[pgsz*i],
                  sizeof(double complex)*pgsz, HPX_NULL);
    }
  }
  free(clocal);

  // The fmm implemented here handles free-space boundary condition,
  // which means boxes of levels greater than or equal to 3 need to
  // wait for local_v.
  double complex *mlocal_v = calloc(pgsz, sizeof(double complex));
  double complex *mlocal_h = calloc(pgsz, sizeof(double complex));

  if (level >= 3) {
    hpx_addr_t parent_local = hpx_lco_future_array_at(local_v, parent);
    double complex *plocal = calloc(pgsz, sizeof(double complex));
    hpx_lco_get(parent_local, plocal, sizeof(double complex)*pgsz);
    local_to_local(plocal, which, mlocal_v);
    free(plocal);
  }

  if (level >= 2) {
    hpx_addr_t my_local_h = hpx_lco_future_array_at(local_h, boxid);
    hpx_lco_get(my_local_h, mlocal_h, sizeof(double complex)*pgsz);
  }

  for (int i = 0; i < pgsz; i++)
    mlocal_v[i] += mlocal_h[i];

  hpx_addr_t my_local_v = hpx_lco_future_array_at(local_v, boxid);
  hpx_lco_set(my_local_v, mlocal_v, sizeof(double complex)*pgsz, HPX_NULL);

  free(mlocal_h);
  free(mlocal_v);

  hpx_thread_continue(0, NULL); 
}


int _process_near_field_action(void *args) {
  // _process_near_field_action handles the lists 1, 3, and 4 for each
  // target box. When processing list 4, parent box must be processed
  // before its child boxes.

  int boxid = *(int *)args; 
  fmm_box_t *mbox = fmm_dag->tboxptrs[boxid];
  int parent = mbox->parent;
  fmm_box_t *pbox = fmm_dag->tboxptrs[boxid];

  if (pbox->list4) {
    hpx_addr_t pbox_near_field_syn =
      hpx_lco_future_array_at(near_field_syn, parent);
    hpx_lco_wait(pbox_near_field_syn);
  }

  // process list 4 for the specified target box
  process_list4(fmm_dag, boxid, sources_pinned, charges_pinned, 
		targets_pinned, potential, field);

  if (mbox->nchild == 0)
    process_list13(fmm_dag, boxid, sources_pinned, charges_pinned, 
		   targets_pinned, potential, field);

  hpx_thread_continue(0, NULL);
}

void source_to_multipole(const double *points, int nsources,
                         const double *center, double scale,
                         double complex *multipole) {

  int pgsz      = fmm_param.pgsz;
  double *ytopc = fmm_param.ytopc;
  int pterms    = fmm_param.pterms;

  const double precision = 1e-14;
  double *powers = calloc(pterms + 1, sizeof(double));
  double *p = calloc(pgsz, sizeof(double));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));

  // source-to-multipole operation
  for (int i = 0; i < nsources; i++) {
    double rx = points[4*i] - center[0];
    double ry = points[4*i + 1] - center[1];
    double rz = points[4*i + 2] - center[2];
    double proj = rx*rx + ry*ry;
    double rr = proj + rz*rz;
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 1.0 : rz/d);
    ephi[0] = (proj <= precision*d ? 1.0 : rx/proj + _Complex_I*ry/proj);
    d *= scale;
    powers[0] = 1.0;
    for (int ell = 1; ell <= pterms; ell++) {
      powers[ell] = powers[ell - 1]*d;
      ephi[ell] = ephi[ell - 1]*ephi[0];
    }

    double charge = points[4*i + 3];
    multipole[0] += charge;

    lgndr(pterms, ctheta, p);
    for (int ell = 1; ell <= pterms; ell++) {
      double cp = charge*powers[ell]*p[ell];
      multipole[ell] += cp;
    }

    for (int m = 1; m <= pterms; m++) {
      int offset = m*(pterms + 1);
      for (int ell = m; ell <= pterms; ell++) {
        double cp = charge*powers[ell]*ytopc[ell + offset]*p[ell + offset];
        multipole[ell + offset] += cp*conj(ephi[m - 1]);
      }
    }
  }

  free(powers);
  free(p);
  free(ephi);
}


void multipole_to_multipole(const double complex *cmpole, int ichild,
                            double complex *pmpole) {
  int pterms = fmm_param.pterms;
  int pgsz   = fmm_param.pgsz;

  const double complex var[5] =
    {1,-1 + _Complex_I, 1 + _Complex_I, 1 - _Complex_I, -1 - _Complex_I};
  const double arg = sqrt(2)/2.0;
  const int iflu[8] = {3, 4, 2, 1, 3, 4, 2, 1};

  double *powers = calloc(pterms + 3, sizeof(double));
  double complex *mpolen = calloc(pgsz, sizeof(double complex));
  double complex *marray = calloc(pgsz, sizeof(double complex));
  double complex *ephi   = calloc(pterms + 3, sizeof(double complex));

  int ifl = iflu[ichild];
  double *rd = (ichild < 4 ? fmm_param.rdsq3 : fmm_param.rdmsq3);
  double *dc = fmm_param.dc;

  ephi[0] = 1.0;
  ephi[1] = arg*var[ifl];
  double dd = -sqrt(3)/2.0;
  powers[0] = 1.0;

  for (int ell = 1; ell <= pterms + 1; ell++) {
    powers[ell] = powers[ell - 1]*dd;
    ephi[ell + 1] = ephi[ell]*ephi[1];
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mpolen[index] = conj(ephi[m])*cmpole[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    int offset1 = (m + pterms)*pgsz;
    int offset2 = (-m + pterms)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      marray[index] = mpolen[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += mpolen[index1]*rd[index1 + offset1] +
                         conj(mpolen[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int k = 0; k <= pterms; k++) {
    int offset = k*(pterms + 1);
    for (int j = k; j <= pterms; j++) {
      int index = offset + j;
      mpolen[index] = marray[index];
      for (int ell = 1; ell <= j - k; ell++) {
        int index2 = j - k + ell*(2*pterms + 1);
        int index3 = j + k + ell*(2*pterms + 1);
        mpolen[index] += marray[index - ell]*powers[ell]*
                         dc[index2]*dc[index3];
      }
    }
  }

  for (int m = 0; m <= pterms; m += 2) {
    int offset = m*(pterms + 1);
    int offset1 = (m + pterms)*pgsz;
    int offset2 = (-m + pterms)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = mpolen[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] -= mpolen[index1]*rd[index1 + offset1] +
                         conj(mpolen[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += mpolen[index1]*rd[index1 + offset1] +
                         conj(mpolen[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int m = 1; m <= pterms; m += 2) {
    int offset = m*(pterms + 1);
    int offset1 = (m + pterms)*pgsz;
    int offset2 = (-m + pterms)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = -mpolen[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += mpolen[index1]*rd[index1 + offset1] +
                         conj(mpolen[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] -= mpolen[index1]*rd[index1 + offset1] +
                         conj(mpolen[index1])*rd[index1 + offset2];
      }
    }
  }

  powers[0] = 1.0;
  for (int ell = 1; ell <= pterms + 1; ell++)
    powers[ell] = powers[ell - 1]/2;

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mpolen[index] = ephi[m]*marray[index]*powers[ell];
    }
  }

  for (int m = 0; m < pgsz; m++)
    pmpole[m] += mpolen[m];

  free(ephi);
  free(powers);
  free(mpolen);
  free(marray);
}


void multipole_to_exponential(const double complex *multipole,
                              double complex *mexpu,
                              double complex *mexpd) {

  int nlambs   = fmm_param.nlambs;
  int nexpmax  = fmm_param.nexpmax;
  int *numfour = fmm_param.numfour;
  int *numphys = fmm_param.numphys;
  int pterms   = fmm_param.pterms;
  int pgsz     = fmm_param.pgsz;
  double *rlsc = fmm_param.rlsc;

  double complex *tempu = calloc(nexpmax, sizeof(double complex));
  double complex *tempd = calloc(nexpmax, sizeof(double complex));

  int ntot = 0;
  for (int nell = 0; nell < nlambs; nell++) {
    double sgn = -1.0;
    double complex zeyep = 1.0;
    for (int mth = 0; mth <= numfour[nell] - 1; mth++) {
      int ncurrent = ntot + mth;
      double complex ztmp1 = 0.0;
      double complex ztmp2 = 0.0;
      sgn = -sgn;
      int offset = mth*(pterms + 1);
      int offset1 = offset + nell*pgsz;
      for (int nm = mth; nm <= pterms; nm += 2)
        ztmp1 += rlsc[nm + offset1]*multipole[nm + offset];
      for (int nm = mth + 1; nm <= pterms; nm += 2)
        ztmp2 += rlsc[nm + offset1]*multipole[nm + offset];

      tempu[ncurrent] = (ztmp1 + ztmp2)*zeyep;
      tempd[ncurrent] = sgn*(ztmp1 - ztmp2)*zeyep;
      zeyep *= _Complex_I;
    }
    ntot += numfour[nell];
  }

  int nftot, nptot, nexte, nexto;
  nftot = 0;
  nptot = 0;
  nexte = 0;
  nexto = 0;

  double complex *fexpe = fmm_param.fexpe;
  double complex *fexpo = fmm_param.fexpo;

  for (int i = 0; i < nlambs; i++) {
    for (int ival = 0; ival < numphys[i]/2; ival++) {
      mexpu[nptot + ival] = tempu[nftot];
      mexpd[nptot + ival] = tempd[nftot];

      for (int nm = 1; nm < numfour[i]; nm += 2) {
        double rt1 = cimag(fexpe[nexte])*creal(tempu[nftot + nm]);
        double rt2 = creal(fexpe[nexte])*cimag(tempu[nftot + nm]);
        double rtmp1 = 2*(rt1 + rt2);

        double rt3 = cimag(fexpe[nexte])*creal(tempd[nftot + nm]);
        double rt4 = creal(fexpe[nexte])*cimag(tempd[nftot + nm]);
        double rtmp2 = 2*(rt3 + rt4);

        nexte++;
        mexpu[nptot + ival] += rtmp1*_Complex_I;
        mexpd[nptot + ival] += rtmp2*_Complex_I;
      }

      for (int nm = 2; nm < numfour[i]; nm += 2) {
        double rt1 = creal(fexpo[nexto])*creal(tempu[nftot + nm]);
        double rt2 = cimag(fexpo[nexto])*cimag(tempu[nftot + nm]);
        double rtmp1 = 2*(rt1 - rt2);

        double rt3 = creal(fexpo[nexto])*creal(tempd[nftot + nm]);
        double rt4 = cimag(fexpo[nexto])*cimag(tempd[nftot + nm]);
        double rtmp2 = 2*(rt3 - rt4);

        nexto++;
        mexpu[nptot + ival] += rtmp1;
        mexpd[nptot + ival] += rtmp2;
      }
    }
    nftot += numfour[i];
    nptot += numphys[i]/2;
  }

  free(tempu);
  free(tempd);
}


void exponential_to_local(const disaggr_nonleaf_arg_t *disaggr_nonleaf_arg,
                          const int child[8], double complex *clocal) {

  int nexpmax        = fmm_param.nexpmax;
  int nexptotp       = fmm_param.nexptotp;
  int pgsz           = fmm_param.pgsz;
  double complex *xs = fmm_param.xs;
  double complex *ys = fmm_param.ys;
  double *zs         = fmm_param.zs;
  double *rdplus     = fmm_param.rdplus;
  double *rdminus    = fmm_param.rdminus;

  double scale = disaggr_nonleaf_arg->scale;

  double complex *work = calloc(nexpmax*16, sizeof(double complex));
  double complex *mw1 = calloc(pgsz, sizeof(double complex));
  double complex *mw2 = calloc(pgsz, sizeof(double complex));
  double complex *mexpf1 = calloc(nexpmax, sizeof(double complex));
  double complex *mexpf2 = calloc(nexpmax, sizeof(double complex));
  double complex *temp   = calloc(nexpmax, sizeof(double complex));

  // process z direction exponential expansions
  double complex *mexuall = work;
  double complex *mexu1234 = work + nexpmax;
  double complex *mexdall = mexu1234 + nexpmax;
  double complex *mexd5678 = mexdall + nexpmax;

  int nuall = disaggr_nonleaf_arg->nuall;
  int nu1234 = disaggr_nonleaf_arg->nu1234;
  int ndall = disaggr_nonleaf_arg->ndall;
  int nd5678 = disaggr_nonleaf_arg->nd5678;

  make_ulist(expd, disaggr_nonleaf_arg->uall, nuall,
             disaggr_nonleaf_arg->xuall, disaggr_nonleaf_arg->yuall, mexuall);
  make_ulist(expd, disaggr_nonleaf_arg->u1234, nu1234,
             disaggr_nonleaf_arg->x1234, disaggr_nonleaf_arg->y1234, mexu1234);
  make_dlist(expu, disaggr_nonleaf_arg->dall, ndall,
             disaggr_nonleaf_arg->xdall, disaggr_nonleaf_arg->ydall, mexdall);
  make_dlist(expu, disaggr_nonleaf_arg->d5678, nd5678,
             disaggr_nonleaf_arg->x5678, disaggr_nonleaf_arg->y5678, mexd5678);

  if (child[0]) {
    double complex *local = &clocal[0];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 2]*scale;
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexu1234[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 1]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[1]) {
    double complex *local = &clocal[pgsz];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 2]*conj(xs[3*j])*scale;
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexu1234[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 1]*xs[3*j]*scale;
      exponential_to_local_p1(temp, mexpf2);
      iexp2++;
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[2]) {
    double complex *local = &clocal[pgsz*2];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 2]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexu1234[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[3]) {
    double complex *local = &clocal[pgsz*3];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 2]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
    }

    if (nu1234) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexu1234[j]*zs[3*j + 1]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 1]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[4]) {
    double complex *local = &clocal[pgsz*4];
    int iexp1 = 0, iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 1]*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 2]*scale;
      iexp2++;
    }

    if (nd5678) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexd5678[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[5]) {
    double complex *local = &clocal[pgsz*5];
    int iexp1 = 0, iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 2]*xs[3*j]*scale;
      iexp2++;
    }

    if (nd5678) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexd5678[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[6]) {
    double complex *local = &clocal[pgsz*6];
    int iexp1 = 0, iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 2]*ys[3*j]*scale;
      iexp2++;
    }

    if (nd5678) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexd5678[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  if (child[7]) {
    double complex *local = &clocal[pgsz*7];
    int iexp1 = 0, iexp2 = 0;

    if (nuall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexuall[j]*zs[3*j + 1]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (ndall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexdall[j]*zs[3*j + 2]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
    }

    if (nd5678) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexd5678[j]*zs[3*j + 1]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw1[j];
    }
  }

  // process y direction exponential expansions
  double complex *mexnall = work;
  double complex *mexn1256 = mexnall + nexpmax;
  double complex *mexn12 = mexn1256 + nexpmax;
  double complex *mexn56 = mexn12 + nexpmax;
  double complex *mexsall = mexn56 + nexpmax;
  double complex *mexs3478 = mexsall + nexpmax;
  double complex *mexs34 = mexs3478 + nexpmax;
  double complex *mexs78 = mexs34 + nexpmax;

  int nnall = disaggr_nonleaf_arg->nnall;
  int nn1256 = disaggr_nonleaf_arg->nn1256;
  int nn12 = disaggr_nonleaf_arg->nn12;
  int nn56 = disaggr_nonleaf_arg->nn56;
  int nsall = disaggr_nonleaf_arg->nsall;
  int ns3478 = disaggr_nonleaf_arg->ns3478;
  int ns34 = disaggr_nonleaf_arg->ns34;
  int ns78 = disaggr_nonleaf_arg->ns78;

  make_ulist(exps, disaggr_nonleaf_arg->nall, nnall,
             disaggr_nonleaf_arg->xnall, disaggr_nonleaf_arg->ynall, mexnall);
  make_ulist(exps, disaggr_nonleaf_arg->n1256, nn1256,
             disaggr_nonleaf_arg->x1256, disaggr_nonleaf_arg->y1256, mexn1256);
  make_ulist(exps, disaggr_nonleaf_arg->n12, nn12,
             disaggr_nonleaf_arg->x12, disaggr_nonleaf_arg->y12, mexn12);
  make_ulist(exps, disaggr_nonleaf_arg->n56, nn56,
             disaggr_nonleaf_arg->x56, disaggr_nonleaf_arg->y56, mexn56);
  make_dlist(expn, disaggr_nonleaf_arg->sall, nsall,
             disaggr_nonleaf_arg->xsall, disaggr_nonleaf_arg->ysall, mexsall);
  make_dlist(expn, disaggr_nonleaf_arg->s3478, ns3478,
             disaggr_nonleaf_arg->x3478, disaggr_nonleaf_arg->y3478, mexs3478);
  make_dlist(expn, disaggr_nonleaf_arg->s34, ns34,
             disaggr_nonleaf_arg->x34, disaggr_nonleaf_arg->y34, mexs34);
  make_dlist(expn, disaggr_nonleaf_arg->s78, ns78,
             disaggr_nonleaf_arg->x78, disaggr_nonleaf_arg->y78, mexs78);

  if (child[0]) {
    double complex *local = &clocal[0];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 2]*scale;
      iexp1++;
    }

    if (nn1256) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn1256[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (nn12) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn12[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 1]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[1]) {
    double complex *local = &clocal[pgsz];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 2]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (nn1256) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn1256[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (nn12) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn12[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[2]) {
    double complex *local = &clocal[pgsz*2];
    int iexp1 = 0, iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 1]*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 2]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs3478[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (ns34) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs34[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[3]) {
    double complex *local = &clocal[pgsz*3];
    int iexp1 = 0, iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 2]*ys[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs3478[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (ns34) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs34[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[4]) {
    double complex *local = &clocal[pgsz*4];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 2]*conj(xs[3*j])*scale;
      iexp1++;
    }

    if (nn1256) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn1256[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp1++;
    }

    if (nn56) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn56[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 1]*xs[3*j]*scale;
      exponential_to_local_p1(temp, mexpf2);
      iexp2++;
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[5]) {
    double complex *local = &clocal[pgsz*5];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 2]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
    }

    if (nn1256) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn1256[j]*zs[3*j + 1]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
    }

    if (nn56) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexn56[j]*zs[3*j + 1]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 1]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[6]) {
    double complex *local = &clocal[pgsz*6];
    int iexp1 = 0, iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 2]*xs[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs3478[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp2++;
    }

    if (ns78) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs78[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[7]) {
    double complex *local = &clocal[pgsz*7];
    int iexp1 = 0, iexp2 = 0;

    if (nnall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexnall[j]*zs[3*j + 1]*conj(xs[3*j]*ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nsall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexsall[j]*zs[3*j + 2]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
    }

    if (ns3478) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs3478[j]*zs[3*j + 1]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
    }

    if (ns78) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexs78[j]*zs[3*j + 1]*xs[3*j]*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      roty2z(mw1, rdplus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  // process x direction exponential expansions
  double complex *mexeall = work;
  double complex *mexe1357 = mexeall + nexpmax;
  double complex *mexe13 = mexe1357 + nexpmax;
  double complex *mexe57 = mexe13 + nexpmax;
  double complex *mexe1 = mexe57 + nexpmax;
  double complex *mexe3 = mexe1 + nexpmax;
  double complex *mexe5 = mexe3 + nexpmax;
  double complex *mexe7 = mexe5 + nexpmax;
  double complex *mexwall = mexe7 + nexpmax;
  double complex *mexw2468 = mexwall + nexpmax;
  double complex *mexw24 = mexw2468 + nexpmax;
  double complex *mexw68 = mexw24 + nexpmax;
  double complex *mexw2 = mexw68 + nexpmax;
  double complex *mexw4 = mexw2 + nexpmax;
  double complex *mexw6 = mexw4 + nexpmax;
  double complex *mexw8 = mexw6 + nexpmax;

  int neall = disaggr_nonleaf_arg->neall;
  int ne1357 = disaggr_nonleaf_arg->ne1357;
  int ne13 = disaggr_nonleaf_arg->ne13;
  int ne57 = disaggr_nonleaf_arg->ne57;
  int ne1 = disaggr_nonleaf_arg->ne1;
  int ne3 = disaggr_nonleaf_arg->ne3;
  int ne5 = disaggr_nonleaf_arg->ne5;
  int ne7 = disaggr_nonleaf_arg->ne7;
  int nwall = disaggr_nonleaf_arg->nwall;
  int nw2468 = disaggr_nonleaf_arg->nw2468;
  int nw24 = disaggr_nonleaf_arg->nw24;
  int nw68 = disaggr_nonleaf_arg->nw68;
  int nw2 = disaggr_nonleaf_arg->nw2;
  int nw4 = disaggr_nonleaf_arg->nw4;
  int nw6 = disaggr_nonleaf_arg->nw6;
  int nw8 = disaggr_nonleaf_arg->nw8;

  make_ulist(expw, disaggr_nonleaf_arg->eall, neall,
             disaggr_nonleaf_arg->xeall, disaggr_nonleaf_arg->yeall, mexeall);
  make_ulist(expw, disaggr_nonleaf_arg->e1357, ne1357,
             disaggr_nonleaf_arg->x1357, disaggr_nonleaf_arg->y1357, mexe1357);
  make_ulist(expw, disaggr_nonleaf_arg->e13, ne13,
             disaggr_nonleaf_arg->x13, disaggr_nonleaf_arg->y13, mexe13);
  make_ulist(expw, disaggr_nonleaf_arg->e57, ne57,
             disaggr_nonleaf_arg->x57, disaggr_nonleaf_arg->y57, mexe57);
  make_ulist(expw, disaggr_nonleaf_arg->e1, ne1,
             disaggr_nonleaf_arg->x1, disaggr_nonleaf_arg->y1, mexe1);
  make_ulist(expw, disaggr_nonleaf_arg->e3, ne3,
             disaggr_nonleaf_arg->x3, disaggr_nonleaf_arg->y3, mexe3);
  make_ulist(expw, disaggr_nonleaf_arg->e5, ne5,
             disaggr_nonleaf_arg->x5, disaggr_nonleaf_arg->y5, mexe5);
  make_ulist(expw, disaggr_nonleaf_arg->e7, ne7,
             disaggr_nonleaf_arg->x7, disaggr_nonleaf_arg->y7, mexe7);
  make_dlist(expe, disaggr_nonleaf_arg->wall, nwall,
             disaggr_nonleaf_arg->xwall, disaggr_nonleaf_arg->ywall, mexwall);
  make_dlist(expe, disaggr_nonleaf_arg->w2468, nw2468,
             disaggr_nonleaf_arg->x2468, disaggr_nonleaf_arg->y2468, mexw2468);
  make_dlist(expe, disaggr_nonleaf_arg->w24, nw24,
             disaggr_nonleaf_arg->x24, disaggr_nonleaf_arg->y24, mexw24);
  make_dlist(expe, disaggr_nonleaf_arg->w68, nw68,
             disaggr_nonleaf_arg->x68, disaggr_nonleaf_arg->y68, mexw68);
  make_dlist(expe, disaggr_nonleaf_arg->w2, nw2,
             disaggr_nonleaf_arg->x2, disaggr_nonleaf_arg->y2, mexw2);
  make_dlist(expe, disaggr_nonleaf_arg->w4, nw4,
             disaggr_nonleaf_arg->x4, disaggr_nonleaf_arg->y4, mexw4);
  make_dlist(expe, disaggr_nonleaf_arg->w6, nw6,
             disaggr_nonleaf_arg->x6, disaggr_nonleaf_arg->y6, mexw6);
  make_dlist(expe, disaggr_nonleaf_arg->w8, nw8,
             disaggr_nonleaf_arg->x8, disaggr_nonleaf_arg->y8, mexw8);

  if (child[0]) {
    double complex *local = &clocal[0];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 2]*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe1357[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (ne13) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe13[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (ne1) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe1[j]*zs[3*j + 1]*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 1]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[1]) {
    double complex *local = &clocal[pgsz];
    int iexp1 = 0, iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 1]*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 2]*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw2468[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (nw24) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw24[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (nw2) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw2[j]*zs[3*j + 1]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[2]) {
    double complex *local = &clocal[pgsz*2];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 2]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe1357[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne13) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe13[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne3) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe3[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[3]) {
    double complex *local = &clocal[pgsz*3];
    int iexp1 = 0, iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 1]*conj(ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 2]*ys[3*j]*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw2468[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (nw24) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw24[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (nw4) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw4[j]*zs[3*j + 1]*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[4]) {
    double complex *local = &clocal[pgsz*4];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 2]*xs[3*j]*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe1357[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp1++;
    }

    if (ne57) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe57[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp1++;
    }

    if (ne5) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe5[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[5]) {
    double complex *local = &clocal[pgsz*5];
    int iexp1 = 0, iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 1]*xs[3*j]*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 2]*conj(xs[3*j])*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw2468[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp2++;
    }

    if (nw68) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw68[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp2++;
    }

    if (nw6) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw6[j]*zs[3*j + 1]*conj(xs[3*j])*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[6]) {
    double complex *local = &clocal[pgsz*6];
    int iexp1 = 0, iexp2 = 0;

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 2]*xs[3*j]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne1357) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe1357[j]*zs[3*j + 1]*xs[3*j]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne57) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe57[j]*zs[3*j + 1]* xs[3*j]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (ne7) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexe7[j]*zs[3*j + 1]*xs[3*j]*conj(ys[3*j])*scale;
      iexp1++;
    }

    if (iexp1)
      exponential_to_local_p1(temp, mexpf1);

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 1]*conj(xs[3*j])*ys[3*j]*scale;
      iexp2++;
      exponential_to_local_p1(temp, mexpf2);
    }

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  if (child[7]) {
    double complex *local = &clocal[pgsz*7];
    int iexp1 = 0, iexp2 = 0;

    if (neall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexeall[j]*zs[3*j + 1]*xs[3*j]*conj(ys[3*j])*scale;
      iexp1++;
      exponential_to_local_p1(temp, mexpf1);
    }

    for (int j = 0; j < nexptotp; j++)
      temp[j] = 0;

    if (nwall) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] = mexwall[j]*zs[3*j + 2]*conj(xs[3*j])*ys[3*j]*scale;
      iexp2++;
    }

    if (nw2468) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw2468[j]*zs[3*j + 1]*conj(xs[3*j])*ys[3*j]*scale;
      iexp2++;
    }

    if (nw68) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw68[j]*zs[3*j + 1]*conj(xs[3*j])*ys[3*j]*scale;
      iexp2++;
    }

    if (nw8) {
      for (int j = 0; j < nexptotp; j++)
        temp[j] += mexw8[j]*zs[3*j + 1]*conj(xs[3*j])*ys[3*j]*scale;
      iexp2++;
    }

    if (iexp2)
      exponential_to_local_p1(temp, mexpf2);

    if (iexp1 + iexp2) {
      exponential_to_local_p2(iexp2, mexpf2, iexp1, mexpf1, mw1);
      rotz2x(mw1, rdminus, mw2);
      for (int j = 0; j < pgsz; j++)
        local[j] += mw2[j];
    }
  }

  free(work);
  free(mw1);
  free(mw2);
  free(mexpf1);
  free(mexpf2);
  free(temp);
}

void exponential_to_local_p1(const double complex *mexpphys,
                             double complex *mexpf) {
  int nlambs = fmm_param.nlambs;
  int *numfour = fmm_param.numfour;
  int *numphys = fmm_param.numphys;
  double complex *fexpback = fmm_param.fexpback;

  int nftot = 0;
  int nptot = 0;
  int next  = 0;

  for (int i = 0; i < nlambs; i++) {
    int nalpha = numphys[i];
    int nalpha2 = nalpha/2;
    mexpf[nftot] = 0;
    for (int ival = 0; ival < nalpha2; ival++) {
      mexpf[nftot] += 2.0*creal(mexpphys[nptot + ival]);
    }
    mexpf[nftot] /= nalpha;

    for (int nm = 2; nm < numfour[i]; nm += 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
        double rtmp = 2*creal(mexpphys[nptot + ival]);
        mexpf[nftot + nm] += fexpback[next]*rtmp;
        next++;
      }
      mexpf[nftot + nm] /= nalpha;
    }

    for (int nm = 1; nm < numfour[i]; nm += 2) {
      mexpf[nftot + nm] = 0;
      for (int ival = 0; ival < nalpha2; ival++) {
        double complex ztmp = 2*cimag(mexpphys[nptot + ival])*_Complex_I;
        mexpf[nftot + nm] += fexpback[next]*ztmp;
        next++;
      }
      mexpf[nftot + nm] /= nalpha;
    }
    nftot += numfour[i];
    nptot += numphys[i]/2;
  }
}


void exponential_to_local_p2(int iexpu, const double complex *mexpu,
                             int iexpd, const double complex *mexpd,
                             double complex *local) {

  int pterms = fmm_param.pterms;
  int nlambs = fmm_param.nlambs;
  int nexptot = fmm_param.nexptot;
  int pgsz = fmm_param.pgsz;
  double *whts = fmm_param.whts;
  double *rlams = fmm_param.rlams;
  int *numfour = fmm_param.numfour;
  double *ytopcs = fmm_param.ytopcs;

  double *rlampow = calloc(pterms + 1, sizeof(double));
  double complex *zeye = calloc(pterms + 1, sizeof(double complex));
  double complex *mexpplus = calloc(nexptot, sizeof(double complex));
  double complex *mexpminus = calloc(nexptot, sizeof(double complex));

  zeye[0] = 1.0;
  for (int i = 1; i <= pterms; i++)
    zeye[i] = zeye[i - 1]*_Complex_I;

  for (int i = 0; i < pgsz; i++)
    local[i] = 0;

  for (int i = 0; i < nexptot; i++) {
    if (iexpu <= 0) {
      mexpplus[i] = mexpd[i];
      mexpminus[i] = mexpd[i];
    } else if (iexpd <= 0) {
      mexpplus[i] = mexpu[i];
      mexpminus[i] = -mexpu[i];
    } else {
      mexpplus[i] = mexpd[i] + mexpu[i];
      mexpminus[i] = mexpd[i] - mexpu[i];
    }
  }

  int ntot = 0;
  for (int nell = 0; nell < nlambs; nell++) {
    rlampow[0] = whts[nell];
    double rmul = rlams[nell];
    for (int j = 1; j <= pterms; j++)
      rlampow[j] = rlampow[j - 1]*rmul;

    int mmax = numfour[nell]-1;
    for (int mth = 0; mth <= mmax; mth += 2) {
      int offset = mth*(pterms + 1);
      for (int nm = mth; nm <= pterms; nm += 2) {
        int index = offset + nm;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul*mexpplus[ncurrent];
      }

      for (int nm = mth + 1; nm <= pterms; nm += 2) {
        int index = offset + nm;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul*mexpminus[ncurrent];
      }
    }

    for (int mth = 1; mth <= mmax; mth += 2) {
      int offset = mth*(pterms + 1);
      for (int nm = mth + 1; nm <= pterms; nm += 2) {
        int index = nm + offset;
        int ncurrent = ntot+mth;
        rmul = rlampow[nm];
        local[index] += rmul*mexpplus[ncurrent];
      }

      for (int nm = mth; nm <= pterms; nm += 2) {
        int index = nm + offset;
        int ncurrent = ntot + mth;
        rmul = rlampow[nm];
        local[index] += rmul*mexpminus[ncurrent];
      }
    }
    ntot += numfour[nell];
  }

  for (int mth = 0; mth <= pterms; mth++) {
    int offset = mth*(pterms + 1);
    for (int nm = mth; nm <= pterms; nm++) {
      int index = nm + offset;
      local[index] *= zeye[mth]*ytopcs[index];
    }
  }
  free(rlampow);
  free(zeye);
  free(mexpplus);
  free(mexpminus);
}


void local_to_local(const double complex *plocal, int which,
                    double complex *local) {
  const double complex var[5] =
  {1, 1 - _Complex_I, -1 - _Complex_I, -1 + _Complex_I, 1 + _Complex_I};
  const double arg = sqrt(2)/2.0;
  const int ifld[8] = {1, 2, 4, 3, 1, 2, 4, 3};

  int pterms = fmm_param.pterms;
  int pgsz   = fmm_param.pgsz;
  double complex *localn = calloc(pgsz, sizeof(double complex));
  double complex *marray = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms, sizeof(double complex));
  double *powers = calloc(1 + pterms, sizeof(double));

  double *rd = (which < 4 ? fmm_param.rdsq3 : fmm_param.rdmsq3);
  double *dc = fmm_param.dc;

  int ifl = ifld[which];
  ephi[0] = 1.0;
  ephi[1] = arg*var[ifl];
  double dd = -sqrt(3)/4.0;
  powers[0] = 1.0;

  for (int ell = 1; ell <= pterms; ell++)
    powers[ell] = powers[ell - 1]*dd;

  for (int ell = 2; ell <= pterms; ell++)
    ephi[ell] = ephi[ell - 1]*ephi[1];

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      localn[index] = conj(ephi[m])*plocal[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    int offset1 = (pterms + m)*pgsz;
    int offset2 = (pterms - m)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += localn[index1]*rd[index1 + offset1] +
                         conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int k = 0; k <= pterms; k++) {
    int offset = k*(pterms + 1);
    for (int j = k; j <= pterms; j++) {
      int index = j + offset;
      localn[index] = marray[index];
      for (int ell = 1; ell <= pterms - j; ell++) {
        int index1 = ell + index;
        int index2 = ell + j + k + ell*(2*pterms + 1);
        int index3 = ell + j - k + ell*(2*pterms + 1);
        localn[index] += marray[index1]*powers[ell]*dc[index2]*dc[index3];
      }
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    int offset1 = (pterms + m)*pgsz;
    int offset2 = (pterms - m)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] -= localn[index1]*rd[index1 + offset1] +
                         conj(localn[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += localn[index1]*rd[index1 + offset1] +
                         conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  for (int m = 1; m <= pterms; m += 2) {
    int offset = m*(pterms + 1);
    int offset1 = (pterms + m)*pgsz;
    int offset2 = (pterms - m)*pgsz;
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      marray[index] = -localn[ell]*rd[ell + offset1];
      for (int mp = 1; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] += localn[index1]*rd[index1 + offset1] +
                         conj(localn[index1])*rd[index1 + offset2];
      }

      for (int mp = 2; mp <= ell; mp += 2) {
        int index1 = ell + mp*(pterms + 1);
        marray[index] -= localn[index1]*rd[index1 + offset1] +
                         conj(localn[index1])*rd[index1 + offset2];
      }
    }
  }

  powers[0] = 1.0;
  for (int ell = 1; ell <= pterms; ell++)
    powers[ell] = powers[ell - 1]/2;

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      localn[index] = ephi[m]*marray[index]*powers[ell];
    }
  }

  for (int m = 0; m < pgsz; m++)
    local[m] += localn[m];

  free(ephi);
  free(powers);
  free(localn);
  free(marray);
}


void local_to_target(const double complex *local, const double *center,
                     double scale, const double *points,
                     int ntargets, double *result) {
  int pgsz = fmm_param.pgsz;
  int pterms = fmm_param.pterms;
  double *ytopc = fmm_param.ytopc;
  double *ytopcs = fmm_param.ytopcs;
  double *ytopcsinv = fmm_param.ytopcsinv;

  double *p = calloc(pgsz, sizeof(double));
  double *powers = calloc(pterms + 1, sizeof(double));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));
  const double precision = 1e-14;

  for (int i = 0; i < ntargets; i++) {
    double *pot = &result[4*i], *field = &result[4*i + 1];
    *pot = 0;
    field[0] = field[1] = field[2] = 0;
    double field0, field1, field2, rloc, cp, rpotz = 0.0;
    double complex cpz, zs1 = 0.0, zs2 = 0.0, zs3 = 0.0;
    double rx = points[3*i] - center[0];
    double ry = points[3*i + 1] - center[1];
    double rz = points[3*i + 2] - center[2];
    double proj = rx*rx + ry*ry;
    double rr = proj + rz*rz;
    proj = sqrt(proj);
    double d = sqrt(rr);
    double ctheta = (d <= precision ? 0.0 : rz/d);
    ephi[0] = (proj <= precision*d ? 1.0 : rx/proj + _Complex_I*ry/proj);
    d *= scale;
    double dd = d;

    powers[0] = 1.0;
    for (int ell = 1; ell <= pterms; ell++) {
      powers[ell] = dd;
      dd *= d;
      ephi[ell] = ephi[ell - 1]*ephi[0];
    }

    lgndr(pterms, ctheta, p);
    *pot += creal(local[0]);

    field2 = 0.0;
    for (int ell = 1; ell <= pterms; ell++) {
      rloc = creal(local[ell]);
      cp = rloc*powers[ell]*p[ell];
      *pot += cp;
      cp = powers[ell - 1]*p[ell - 1]*ytopcs[ell - 1];
      cpz = local[ell + pterms + 1]*cp*ytopcsinv[ell + pterms + 1];
      zs2 = zs2 + cpz;
      cp = rloc*cp*ytopcsinv[ell];
      field2 += cp;
    }

    for (int ell = 1; ell <= pterms; ell++) {
      for (int m = 1; m <= ell; m++) {
        int index = ell + m*(pterms + 1);
        cpz = local[index]*ephi[m - 1];
        rpotz += creal(cpz)*powers[ell]*ytopc[index]*p[index];
      }

      for (int m = 1; m <= ell - 1; m++) {
        int index1 = ell + m*(pterms + 1);
        int index2 = index1 - 1;
        zs3 += local[index1]*ephi[m - 1]*powers[ell - 1]*
               ytopc[index2]*p[index2]*ytopcs[index2]*ytopcsinv[index1];
      }

      for (int m = 2; m <= ell; m++) {
        int index1 = ell + m*(pterms + 1);
        int index2 = ell - 1 + (m - 1)*(pterms + 1);
        zs2 += local[index1]*ephi[m - 2]*ytopcs[index2]*
               ytopcsinv[index1]*powers[ell - 1]*ytopc[index2]*p[index2];
      }

      for (int m = 0; m <= ell - 2; m++) {
        int index1 = ell + m*(pterms + 1);
        int index2 = ell - 1 + (m + 1)*(pterms + 1);
        zs1 += local[index1]*ephi[m]*ytopcs[index2]*
               ytopcsinv[index1]*powers[ell - 1]*ytopc[index2]*p[index2];
      }
    }

    *pot += 2.0*rpotz;
    field0 = creal(zs2 - zs1);
    field1 = -cimag(zs2 + zs1);
    field2 += 2.0*creal(zs3);

    field[0] += field0*scale;
    field[1] += field1*scale;
    field[2] -= field2*scale;
  }
  free(powers);
  free(ephi);
  free(p);
}


void process_list4(const fmm_dag_t *fmm_dag, int boxid,
                   const double *sources, const double *charges,
                   const double *targets, double *potential, double *field) {

  fmm_box_t *tbox = fmm_dag->tboxptrs[boxid];
  if (tbox->list4) {
    int nlist4 = tbox->list4[0];
    int *list4 = &tbox->list4[1];
    int t_addr = tbox->addr;
    int ntargets = tbox->npts;

    double *my_potential = calloc(ntargets, sizeof(double));
    double *my_field = calloc(ntargets*3, sizeof(double));

    for (int i = 0; i < nlist4; i++) {
      fmm_box_t *sbox = fmm_dag->sboxptrs[list4[i]];
      int s_addr = sbox->addr;
      int nsources = sbox->npts;
      direct_evaluation(&sources[3*s_addr], &charges[s_addr], nsources,
                        &targets[3*t_addr], ntargets,
                        my_potential, my_field);
    }

    int *maptar = fmm_dag->maptar;
    for (int i = 0; i < ntargets; i++) {
      int j = maptar[t_addr + i], j3 = j*3, i3 = i*3;
      potential[j] += my_potential[i];
      field[j3] += my_field[i3];
      field[j3 + 1] += my_field[i3 + 1];
      field[j3 + 2] += my_field[i3 + 2];
    }

    free(my_potential);
    free(my_field);
  }
}


void process_list13(const fmm_dag_t *fmm_dag, int boxid,
                    const double *sources, const double *charges,
                    const double *targets, double *potential, double *field) {
  fmm_box_t *tbox = fmm_dag->tboxptrs[boxid];
  int t_addr = tbox->addr;
  int ntargets = tbox->npts;

  double *my_potential = calloc(ntargets, sizeof(double));
  double *my_field = calloc(ntargets*3, sizeof(double));


  if (tbox->list3) {
    int nlist3 = tbox->list3[0];
    int *list3 = &tbox->list3[1];
    for (int i = 0; i < nlist3; i++) {
      fmm_box_t *sbox = fmm_dag->sboxptrs[list3[i]];
      int s_addr = sbox->addr;
      int nsources = sbox->npts;
      direct_evaluation(&sources[3*s_addr], &charges[s_addr], nsources,
                        &targets[3*t_addr], ntargets,
                        my_potential, my_field);
    }
  }

  if (tbox->list1 && tbox->nchild == 0) {
    int nlist1 = tbox->list1[0];
    int *list1 = &tbox->list1[1];
    for (int i = 0; i < nlist1; i++) {
      fmm_box_t *sbox = fmm_dag->sboxptrs[list1[i]];
      int s_addr = sbox->addr;
      int nsources = sbox->npts;
      direct_evaluation(&sources[3*s_addr], &charges[s_addr], nsources,
                        &targets[3*t_addr], ntargets,
                        my_potential, my_field);
    }
  }

  int *maptar = fmm_dag->maptar;
  for (int i = 0; i < ntargets; i++) {
    int j = maptar[t_addr + i], j3 = j*3, i3 = i*3;
    potential[j] += my_potential[i];
    field[j3] += my_field[i3];
    field[j3 + 1] += my_field[i3 + 1];
    field[j3 + 2] += my_field[i3 + 2];
  }

  free(my_potential);
  free(my_field);
}

void direct_evaluation(const double *sources, const double *charges,
                       int nsources, const double *targets, int ntargets,
                       double *potential, double *field) {
  for (int i = 0; i < ntargets; i++) {
    double pot = 0, fx = 0, fy = 0, fz = 0;
    for (int j = 0; j < nsources; j++) {
      double rx = targets[i*3] - sources[j*3];
      double ry = targets[i*3 + 1] - sources[j*3 + 1];
      double rz = targets[i*3 + 2] - sources[j*3 + 2];
      double q = charges[j];
      double rr = rx*rx + ry*ry + rz*rz;
      double rdis = sqrt(rr);

      if (rr) {
        pot += q/rdis;
        double rmul = q/(rdis*rr);
        fx += rmul*rx;
        fy += rmul*ry;
        fz += rmul*rz;
      }
    }

    potential[i]   += pot;
    field[i*3]     += fx;
    field[i*3 + 1] += fy;
    field[i*3 + 2] += fz;
  }
}


void rotz2y(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param.pterms;
  int pgsz   = fmm_param.pgsz;

  double complex *mwork = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(pterms + 1, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m =1; m <= pterms; m++)
    ephi[m] = -ephi[m - 1]*_Complex_I;

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = offset + ell;
      mwork[index] = ephi[m]*multipole[index];
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mrotate[index] = mwork[ell]*rd[ell + (m + pterms)*pgsz];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp*(pterms + 1);
        mrotate[index] +=
        mwork[index1]*rd[ell + mp*(pterms + 1) + (m + pterms)*pgsz] +
        conj(mwork[index1])*rd[ell + mp*(pterms + 1) + (pterms - m)*pgsz];
      }
    }
  }

  free(ephi);
  free(mwork);
}


void roty2z(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param.pterms;
  int pgsz   = fmm_param.pgsz;

  double complex *mwork = calloc(pgsz, sizeof(double complex));
  double complex *ephi = calloc(1 + pterms, sizeof(double complex));

  ephi[0] = 1.0;
  for (int m = 1; m <= pterms; m++)
    ephi[m] = ephi[m - 1]*_Complex_I;

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mwork[index] = multipole[ell]*rd[ell + (m + pterms)*pgsz];
      for (int mp = 1; mp <= ell; mp++) {
        int index1 = ell + mp*(pterms + 1);
        double complex temp = multipole[index1];
        mwork[index] +=
        temp*rd[ell + mp*(pterms + 1) + (m + pterms)*pgsz] +
        conj(temp)*rd[ell + mp*(pterms + 1) + (pterms - m)*pgsz];
      }
    }
  }

  for (int m = 0; m <= pterms; m++) {
    int offset = m*(pterms + 1);
    for (int ell = m; ell <= pterms; ell++) {
      int index = ell + offset;
      mrotate[index] = ephi[m]*mwork[index];
    }
  }

  free(ephi);
  free(mwork);
}


void rotz2x(const double complex *multipole, const double *rd,
            double complex *mrotate) {
  int pterms = fmm_param.pterms;
  int pgsz   = fmm_param.pgsz;

  int offset1 = pterms*pgsz;
  for (int m = 0; m <= pterms; m++) {
    int offset2 = m*(pterms + 1);
    int offset3 = m*pgsz + offset1;
    int offset4 = -m*pgsz + offset1;
    for (int ell = m; ell <= pterms; ell++) {
      mrotate[ell + offset2] = multipole[ell]*rd[ell + offset3];
      for (int mp = 1; mp <= ell; mp++) {
        int offset5 = mp*(pterms + 1);
        mrotate[ell + offset2] +=
        multipole[ell + offset5]*rd[ell + offset3 + offset5] +
        conj(multipole[ell + offset5])*rd[ell + offset4 + offset5];
      }
    }
  }
}


void lgndr(int nmax, double x, double *y) {
  int n;
  n = (nmax + 1)*(nmax + 1);
  for (int m = 0; m < n; m++)
    y[m] = 0.0;

  double u = -sqrt(1 - x*x);
  y[0] = 1;

  y[1] = x*y[0];
  for (int n = 2; n <= nmax; n++)
    y[n] = ((2*n - 1)*x*y[n - 1] - (n - 1)*y[n - 2])/n;

  int offset1 = nmax + 2;
  for (int m = 1; m <= nmax - 1; m++) {
    int offset2 = m*offset1;
    y[offset2] = y[offset2 - offset1]*u*(2*m - 1);
    y[offset2 + 1] = y[offset2]*x*(2*m + 1);
    for (int n = m + 2; n <= nmax; n++) {
      int offset3 = n + m*(nmax + 1);
      y[offset3] = ((2*n - 1)*x*y[offset3 - 1] -
                    (n + m - 1)*y[offset3 - 2])/(n - m);
    }
  }

  y[nmax + nmax*(nmax + 1)] =
  y[nmax - 1 + (nmax - 1)*(nmax + 1)]*u*(2*nmax - 1);
}


void make_ulist(hpx_addr_t expo, const int *list, int nlist,
                const int *xoff, const int *yoff, double complex *mexpo) {
  int nexpmax = fmm_param.nexpmax;

  for (int i = 0; i < nexpmax; i++)
    mexpo[i] = 0;

  if (nlist) {
    int nexptotp = fmm_param.nexptotp;
    double complex *xs = fmm_param.xs;
    double complex *ys = fmm_param.ys;
    double complex *temp = calloc(nexpmax, sizeof(double complex));

    // get all the exponential expansions
    for (int i = 0; i < nlist; i++) {
      int sboxid = list[i];
      hpx_addr_t sbox_fut = hpx_lco_future_array_at(expo, sboxid);
      hpx_lco_get(sbox_fut, temp,  sizeof(double complex)*nexpmax);

      for (int j = 0; j < nexptotp; j++) {
        double complex zmul = 1;
        if (xoff[i] > 0) {
          zmul *= xs[3*j + xoff[i] - 1];
        } else if (xoff[i] < 0) {
          zmul *= conj(xs[3*j - xoff[i] - 1]);
        }

        if (yoff[i] > 0) {
          zmul *= ys[3*j + yoff[i] - 1];
        } else if (yoff[i] < 0) {
          zmul *= conj(ys[3*j - yoff[i] - 1]);
        }

        mexpo[j] += zmul*temp[j];
      }
    }
    free(temp);
  }
}


void make_dlist(hpx_addr_t expo, const int *list, int nlist,
                const int *xoff, const int *yoff, double complex *mexpo) {
  int nexpmax = fmm_param.nexpmax;

  for (int i = 0; i < nexpmax; i++)
    mexpo[i] = 0;

  if (nlist) {
    int nexptotp = fmm_param.nexptotp;
    double complex *xs = fmm_param.xs;
    double complex *ys = fmm_param.ys;
    double complex *temp = calloc(nexpmax, sizeof(double complex));

    // get all the exponential expansions
    for (int i = 0; i < nlist; i++) {
      int sboxid = list[i];
      hpx_addr_t sbox_fut = hpx_lco_future_array_at(expo, sboxid);
      hpx_lco_get(sbox_fut, temp,  sizeof(double complex)*nexpmax);

      for (int j = 0; j < nexptotp; j++) {
        double complex zmul = 1;
        if (xoff[i] > 0) {
          zmul *= conj(xs[3*j + xoff[i] - 1]);
        } else if (xoff[i] < 0) {
          zmul *= xs[3*j - xoff[i] - 1];
        }

        if (yoff[i] > 0) {
          zmul *= conj(ys[3*j + yoff[i] - 1]);
        } else if (yoff[i] < 0) {
          zmul *= ys[3*j - yoff[i] - 1];
        }

        mexpo[j] += zmul*temp[j];
      }
    }
    free(temp);
  }
}

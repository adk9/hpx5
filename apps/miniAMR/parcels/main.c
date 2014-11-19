#include "main.h"

static hpx_action_t _main          = 0;
static hpx_action_t _advance       = 0;
static hpx_action_t _initDomain    = 0;
hpx_action_t _plot_result = 0;
hpx_action_t _plot_sends = 0;
hpx_action_t _comm_refine_result = 0;
hpx_action_t _comm_refine_sends = 0;
hpx_action_t _comm_reverse_refine_result = 0;
hpx_action_t _comm_reverse_refine_sends = 0;
hpx_action_t _comm_parent_result = 0;
hpx_action_t _comm_parent_sends = 0;
hpx_action_t _comm_parent_reverse_result = 0;
hpx_action_t _comm_parent_reverse_sends = 0;
hpx_action_t _comm_proc_result = 0;
hpx_action_t _comm_proc_sends = 0;
hpx_action_t _comm_parent_proc_result = 0;
hpx_action_t _comm_parent_proc_sends = 0;

static void initdouble(double *input, const size_t size) {
  assert(sizeof(double) == size);
  *input = 0.0;
}

static void sumdouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  *output += *input;
  return;
}

static void initint(int *input, const size_t size) {
  int i;
  int nx = size/sizeof(int);
  for (i=0;i<nx;i++) {
    input[i] = 0;
  }
}

static void sumint(int *output,const int *input, const size_t size) {
  int i;
  int nx = size/sizeof(int);
  for (i=0;i<nx;i++) {
    output[i] += input[i];
  }
  return;
}

static void singleinitintmax(int *input, const size_t size) {
  assert(sizeof(int) == size);
  *input = -99999;
}

static void singleinitintmin(int *input, const size_t size) {
  assert(sizeof(int) == size);
  *input = 99999;
}

static void singleinitint(int *input, const size_t size) {
  assert(sizeof(int) == size);
  *input = 0;
}

static void singlesumint(int *output,const int *input, const size_t size) {
  assert(sizeof(int) == size);
  *output += *input;
  return;
}

static void singleminint(int *output,const int *input, const size_t size) {
  assert(sizeof(int) == size);
  if ( *input < *output ) {
    *output = *input;
  }
  return;
}

static void singlemaxint(int *output,const int *input, const size_t size) {
  assert(sizeof(int) == size);
  if ( *input > *output ) {
    *output = *input;
  }
  return;
}

static int _advance_action(unsigned long *epoch) {
  const unsigned long n = *epoch;
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  if ( ld->ts > ld->num_tsteps ) {
    hpx_gas_unpin(local);
    hpx_lco_set(ld->complete, 0, NULL, HPX_NULL, HPX_NULL);
    return HPX_SUCCESS;
  }

  if ( ld->ts == 0 ) {
    ld->t1 = hpx_time_now();
    if (ld->num_refine || ld->uniform_refine) refine(0,ld,n);
    ld->t2 = hpx_time_now();
    ld->timer_refine_all += hpx_time_diff_ms(ld->t1,ld->t2);

    if (ld->plot_freq) plot(0,ld,n);
    ld->t3 = hpx_time_now();
    ld->timer_plot += hpx_time_diff_ms(ld->t2,ld->t3);

    ld->nb_min = ld->nb_max = ld->global_active;
  }

  ld->ts++;

// don't need this domain to be pinned anymore---let it move
  hpx_gas_unpin(local);

  // 5. spawn the next epoch
  unsigned long next = n + 1;
  return hpx_call(local, _advance, &next, sizeof(next), HPX_NULL);
}


static int _initDomain_action(InitArgs *init) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;
  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  ld->ts = 0;
  ld->complete = init->complete;
  ld->gsum = init->gsum;
  ld->rsum = init->rsum;
  ld->refinelevel = init->refinelevel;
  ld->refinelevel_min = init->refinelevel_min;
  ld->refinelevel_max = init->refinelevel_max;
  ld->rcb_sumint = init->rcb_sumint;
  ld->initallgather = init->initallgather;
  ld->my_pe = init->rank;
  ld->num_pes = init->ndoms;
  int *params = init->params;
  ld->sem_plot = hpx_lco_sema_new(1);
  ld->plot_and[0] = hpx_lco_and_new(ld->num_pes-1);
  ld->plot_and[1] = HPX_NULL;
  ld->sem_refine = hpx_lco_sema_new(1);
  ld->sem_comm_proc = hpx_lco_sema_new(1);
  ld->sem_parent = hpx_lco_sema_new(1);
  ld->sem_comm_parent_proc = hpx_lco_sema_new(1);
  ld->sem_parent_reverse = hpx_lco_sema_new(1);
  ld->objectsize = init->objectsize;
  ld->objects = init->objects;

  ld->epoch = hpx_lco_gencount_new(0);

  int max_num_blocks = params[ 0];
  int target_active = params[ 1];
  int num_refine = params[ 2];
  int uniform_refine = params[ 3];
  int x_block_size = params[ 4];
  int y_block_size = params[ 5];
  int z_block_size = params[ 6];
  int num_vars = params[ 7];
  int comm_vars = params[ 8];
  int init_block_x = params[ 9];
  int init_block_y = params[10];
  int init_block_z = params[11];
  int reorder = params[12];
  int npx = params[13];
  int npy = params[14];
  int npz = params[15];
  int inbalance = params[16];
  int refine_freq = params[17];
  int report_diffusion = params[18];
  int error_tol = params[19];
  int num_tsteps = params[20];
  int stencil = params[21];
  int report_perf = params[22];
  int plot_freq = params[23];
  int num_objects = params[24];
  int checksum_freq = params[25];
  int target_max = params[26];
  int target_min = params[27];
  int stages_per_ts = params[28];
  int lb_opt = params[29];
  int block_change = params[30];
  int code = params[31];
  int permute = params[32];
  int num_pes = params[33];

  ld->max_num_blocks = params[ 0];
  ld->target_active = params[ 1];
  ld->num_refine = params[ 2];
  ld->uniform_refine = params[ 3];
  ld->x_block_size = params[ 4];
  ld->y_block_size = params[ 5];
  ld->z_block_size = params[ 6];
  ld->num_vars = params[ 7];
  ld->comm_vars = params[ 8];
  ld->init_block_x = params[ 9];
  ld->init_block_y = params[10];
  ld->init_block_z = params[11];
  ld->reorder = params[12];
  ld->npx = params[13];
  ld->npy = params[14];
  ld->npz = params[15];
  ld->inbalance = params[16];
  ld->refine_freq = params[17];
  ld->report_diffusion = params[18];
  ld->error_tol = params[19];
  ld->num_tsteps = params[20];
  ld->stencil = params[21];
  ld->report_perf = params[22];
  ld->plot_freq = params[23];
  ld->num_objects = params[24];
  ld->checksum_freq = params[25];
  ld->target_max = params[26];
  ld->target_min = params[27];
  ld->stages_per_ts = params[28];
  ld->lb_opt = params[29];
  ld->block_change = params[30];
  ld->code = params[31];
  ld->permute = params[32];
  ld->num_pes = params[33];

  ld->colors = malloc(ld->num_pes*sizeof(int));

  ld->num_blocks = (int *) malloc((num_refine+1)*sizeof(int));
  ld->num_blocks[0] = num_pes*init_block_x*init_block_y*init_block_z;

  ld->local_num_blocks = (int *) malloc((num_refine+1)*sizeof(int));
  ld->local_num_blocks[0] = init_block_x*init_block_y*init_block_z;

  ld->blocks = (block *) malloc(max_num_blocks*sizeof(block));

  int n,m,i,j,k;
  for (n = 0; n < max_num_blocks; n++) {
      ld->blocks[n].number = -1;
      ld->blocks[n].array = (double ****) malloc(num_vars*sizeof(double ***));
      for (m = 0; m < num_vars; m++) {
         ld->blocks[n].array[m] = (double ***) malloc((x_block_size+2)*sizeof(double **));
         for (i = 0; i < x_block_size+2; i++) {
            ld->blocks[n].array[m][i] = (double **) malloc((y_block_size+2)*sizeof(double *));
            for (j = 0; j < y_block_size+2; j++)
               ld->blocks[n].array[m][i][j] = (double *) malloc((z_block_size+2)*sizeof(double));
         }
      }
  }

  ld->sorted_list = (sorted_block *) malloc(max_num_blocks*sizeof(sorted_block));
  ld->sorted_index = (int *) malloc((num_refine+2)*sizeof(int));

  ld->max_num_parents = max_num_blocks;  // Guess at number needed
  ld->parents = (parent *) malloc(ld->max_num_parents*sizeof(parent));
  for (n = 0; n < ld->max_num_parents; n++)
      ld->parents[n].number = -1;

  ld->max_num_dots = 2*max_num_blocks;     // Guess at number needed
  ld->dots = (dot *) malloc(ld->max_num_dots*sizeof(dot));
  for (n = 0; n < ld->max_num_dots; n++)
      ld->dots[n].number = -1;

  ld->grid_sum = (double *)malloc(num_vars*sizeof(double));

  ld->p8 = (int *) malloc((num_refine+2)*sizeof(int));
  ld->p2 = (int *) malloc((num_refine+2)*sizeof(int));
  ld->block_start = (int *) malloc((num_refine+1)*sizeof(int));

  ld->from = (int *) malloc(num_pes*sizeof(int));
  ld->to   = (int *) malloc(num_pes*sizeof(int));

  // first try at allocating comm arrays
  for (i = 0; i < 3; i++) {
     if (num_refine)
         ld->max_comm_part[i] = 20;
      else
         ld->max_comm_part[i] = 2;
      ld->comm_partner[i] = (int *) malloc(ld->max_comm_part[i]*sizeof(int));
      ld->send_size[i] = (int *) malloc(ld->max_comm_part[i]*sizeof(int));
      ld->recv_size[i] = (int *) malloc(ld->max_comm_part[i]*sizeof(int));
      ld->comm_index[i] = (int *) malloc(ld->max_comm_part[i]*sizeof(int));
      ld->comm_num[i] = (int *) malloc(ld->max_comm_part[i]*sizeof(int));
      if (num_refine)
         ld->max_num_cases[i] = 100;
      else if (i == 0)
         ld->max_num_cases[i] = 2*init_block_y*init_block_z;
      else if (i == 1)
         ld->max_num_cases[i] = 2*init_block_x*init_block_z;
      else
         ld->max_num_cases[i] = 2*init_block_x*init_block_y;
      ld->comm_block[i] = (int *) malloc(ld->max_num_cases[i]*sizeof(int));
      ld->comm_face_case[i] = (int *) malloc(ld->max_num_cases[i]*sizeof(int));
      ld->comm_pos[i] = (int *) malloc(ld->max_num_cases[i]*sizeof(int));
      ld->comm_pos1[i] = (int *)malloc(ld->max_num_cases[i]*sizeof(int));
      ld->comm_send_off[i] = (int *) malloc(ld->max_num_cases[i]*sizeof(int));
      ld->comm_recv_off[i] = (int *) malloc(ld->max_num_cases[i]*sizeof(int));
  }

  if (num_refine) {
      ld->par_b.max_part = 10;
      ld->par_b.max_cases = 100;
      ld->par_p.max_part = 10;
      ld->par_p.max_cases = 100;
      ld->par_p1.max_part = 10;
      ld->par_p1.max_cases = 100;
   } else {
      ld->par_b.max_part = 1;
      ld->par_b.max_cases = 1;
      ld->par_p.max_part = 1;
      ld->par_p.max_cases = 1;
      ld->par_p1.max_part = 1;
      ld->par_p1.max_cases = 1;
   }
   ld->par_b.comm_part = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_b.comm_num = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_b.index = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_b.comm_b = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_b.comm_p = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_b.comm_c = (int *) malloc(ld->par_b.max_cases*sizeof(int));

   ld->par_p.comm_part = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p.comm_num = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p.index = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p.comm_b = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_p.comm_p = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_p.comm_c = (int *) malloc(ld->par_b.max_cases*sizeof(int));

   ld->par_p1.comm_part = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p1.comm_num = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p1.index = (int *) malloc(ld->par_b.max_part*sizeof(int));
   ld->par_p1.comm_b = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_p1.comm_p = (int *) malloc(ld->par_b.max_cases*sizeof(int));
   ld->par_p1.comm_c = (int *) malloc(ld->par_b.max_cases*sizeof(int));

  if (num_refine) {
      ld->s_buf_size = (int) (0.10*((double)max_num_blocks))*comm_vars*
                   (x_block_size+2)*(y_block_size+2)*(z_block_size+2);
      if (ld->s_buf_size < (num_vars*x_block_size*y_block_size*z_block_size + 45))
         ld->s_buf_size = num_vars*x_block_size*y_block_size*z_block_size + 45;
      ld->r_buf_size = 5*ld->s_buf_size;
   } else {
      i = init_block_x*(x_block_size+2);
      j = init_block_y*(y_block_size+2);
      k = init_block_z*(z_block_size+2);
      if (i > j)         // do not need ordering just two largest
         if (j > k)      // i > j > k
            ld->s_buf_size = i*j;
         else            // i > j && k > j
            ld->s_buf_size = i*k;
      else if (i > k)    // j > i > k
            ld->s_buf_size = i*j;
         else            // j > i && k > i
            ld->s_buf_size = j*k;
      ld->r_buf_size = 2*ld->s_buf_size;
   }
   ld->send_buff = (double *) malloc(ld->s_buf_size*sizeof(double));
   ld->recv_buff = (double *) malloc(ld->r_buf_size*sizeof(double));

  // for plot output
  if (ld->my_pe == 0) {
    ld->plot_buf = (int **) malloc(ld->num_pes*sizeof(int *));
    ld->plot_buf_size = (int *) malloc(ld->num_pes*sizeof(int));
  }

  init_profile(ld);
  init_amr(ld);

  // find out how many "receives"
  int nrecvs = 0;
  int dir;
   for (dir = 0; dir < 3; dir++) {
     nrecvs += ld->num_comm_partners[dir];
   }

  if ( ld->num_refine >= ld->block_change ) {
    ld->refine_and_size = ld->num_refine;
  } else {
    ld->refine_and_size = ld->block_change;
  }
  // could be called as many as 8 times in refine_level
  ld->refine_and_size *= 8;

  ld->refine_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  ld->comm_proc_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  ld->reverse_refine_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  ld->parent_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  ld->comm_parent_proc_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  ld->parent_reverse_and = (hpx_addr_t *) malloc(2*ld->refine_and_size * sizeof(hpx_addr_t));
  for (j=0;j<ld->refine_and_size;j++) {
    ld->refine_and[0 + 2*j] = hpx_lco_and_new(nrecvs);
    ld->comm_proc_and[0 + 2*j] = hpx_lco_and_new(nrecvs);
    ld->reverse_refine_and[0 + 2*j] = hpx_lco_and_new(nrecvs);
    ld->parent_and[0 + 2*j] = hpx_lco_and_new(ld->par_p.num_comm_part);
    ld->comm_parent_proc_and[0 + 2*j] = hpx_lco_and_new(ld->par_p.num_comm_part);
    ld->parent_reverse_and[0 + 2*j] = hpx_lco_and_new(ld->par_b.num_comm_part);
  }

  ld->counter_malloc_init = ld->counter_malloc;
  ld->size_malloc_init = ld->size_malloc;

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

static int _main_action(RunArgs *runargs)
{
  hpx_time_t t1 = hpx_time_now();
  int k;

  int nDoms = runargs->params[33];
  int num_refine = runargs->params[ 2];

  hpx_addr_t domain = hpx_gas_global_alloc(nDoms,sizeof(Domain));
  hpx_addr_t init = hpx_lco_and_new(nDoms);
  hpx_addr_t complete = hpx_lco_and_new(nDoms);
  hpx_addr_t gsum = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(double),
                                           (hpx_commutative_associative_op_t)sumdouble,
                                           (void (*)(void *, const size_t size)) initdouble);
  hpx_addr_t rsum = hpx_lco_allreduce_new(nDoms, nDoms, (num_refine+1)*sizeof(int),
                                           (hpx_commutative_associative_op_t)sumint,
                                           (void (*)(void *, const size_t size)) initint);
  hpx_addr_t refinelevel = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(int),
                                           (hpx_commutative_associative_op_t)singlesumint,
                                           (void (*)(void *, const size_t size)) singleinitint);
  hpx_addr_t refinelevel_max = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(int),
                                           (hpx_commutative_associative_op_t)singlemaxint,
                                           (void (*)(void *, const size_t size)) singleinitintmin);
  hpx_addr_t refinelevel_min = hpx_lco_allreduce_new(nDoms, nDoms, sizeof(int),
                                           (hpx_commutative_associative_op_t)singleminint,
                                           (void (*)(void *, const size_t size)) singleinitintmax);
  hpx_addr_t rcb_sumint = hpx_lco_allreduce_new(nDoms, nDoms, (nDoms)*sizeof(int),
                                           (hpx_commutative_associative_op_t)sumint,
                                           (void (*)(void *, const size_t size)) initint);

  hpx_addr_t initallgather = hpx_lco_allgather_new(nDoms, sizeof(int));

  int i;

  // initialize an InitArgs structure with the data from the RunArgs
  // structure---this requires dynamic allocation because the number of in-place
  // objects in the variable length "objects" array is a runtime value
  InitArgs *args = malloc(sizeof(*args) +
                          sizeof(object) * runargs->objectsize);
  args->complete = complete;
  args->gsum = gsum;
  args->rsum = rsum;
  args->refinelevel = refinelevel;
  args->refinelevel_max = refinelevel_max;
  args->refinelevel_min = refinelevel_min;
  args->rcb_sumint = rcb_sumint;
  args->initallgather = initallgather;
  memcpy(&args->params, runargs->params, 34 * sizeof(int));
  args->objectsize = runargs->objectsize;
  memcpy(&args->objects, &runargs->objects,
         sizeof(object) * runargs->objectsize);

  for (k=0;k<nDoms;k++) {
    // Each time through this loop, update the rank in the data we'll send
    // out. We can reuse the buffer because hpx_call is locally synchronous. If
    // we were using hpx_call_async, or a parcel_send, we'd need to wait for the
    // send to complete locally to reuse the buffer.
    //
    args->rank = k;
    args->ndoms = nDoms;
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k, sizeof(Domain));
    hpx_call(block, _initDomain, args, sizeof(InitArgs) + sizeof(object) * runargs->objectsize, init);
  }
  hpx_lco_wait(init);
  hpx_lco_delete(init, HPX_NULL);
  free(args);
#if 0
  // Spawn the first epoch, _advanceDomain will recursively spawn each epoch.
  unsigned long epoch = 0;

  for (k=0;k<nDoms;k++) {
    hpx_addr_t block = hpx_addr_add(domain, sizeof(Domain) * k, sizeof(Domain));
    hpx_call(block, _advance, &epoch, sizeof(epoch), HPX_NULL);
  }

  // And wait for each domain to reach the end of its simulation
  hpx_lco_wait(complete);
  hpx_lco_delete(complete, HPX_NULL);
#endif
  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g Num domains: %d\n",elapsed,nDoms);
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t--help, show help\n");
   printf("(Optional) command line input is of the form: \n\n");

   printf("--num_pes controls granularity\n");
   printf("--nx - block size x (even && > 0)\n");
   printf("--ny - block size y (even && > 0)\n");
   printf("--nz - block size z (even && > 0)\n");
   printf("--init_x - initial blocks in x (> 0)\n");
   printf("--init_y - initial blocks in y (> 0)\n");
   printf("--init_z - initial blocks in z (> 0)\n");
   printf("--reorder - ordering of blocks if initial number > 1\n");
   printf("--npx - (0 < npx <= num_pes)\n");
   printf("--npy - (0 < npy <= num_pes)\n");
   printf("--npz - (0 < npz <= num_pes)\n");
   printf("--max_blocks - maximun number of blocks per core\n");
   printf("--num_refine - (>= 0) number of levels of refinement\n");
   printf("--block_change - (>= 0) number of levels a block can change in a timestep\n");
   printf("--uniform_refine - if 1, then grid is uniformly refined\n");
   printf("--refine_freq - frequency (in timesteps) of checking for refinement\n");
   printf("--target_active - (>= 0) target number of blocks per core, none if 0\n");
   printf("--target_max - (>= 0) max number of blocks per core, none if 0\n");
   printf("--target_min - (>= 0) min number of blocks per core, none if 0\n");
   printf("--inbalance - percentage inbalance to trigger inbalance\n");
   printf("--lb_opt - load balancing - 0 = none, 1 = each refine, 2 = each refine phase\n");
   printf("--num_vars - number of variables (> 0)\n");
   printf("--comm_vars - number of vars to communicate together\n");
   printf("--num_tsteps - number of timesteps (> 0)\n");
   printf("--stages_per_ts - number of comm/calc stages per timestep\n");
   printf("--checksum_freq - number of stages between checksums\n");
   printf("--stencil - 7 or 27 point (27 will not work with refinement (except uniform))\n");
   printf("--error_tol - (e^{-error_tol} ; >= 0) \n");
   printf("--report_diffusion - (>= 0) none if 0\n");
   printf("--report_perf - 0, 1, 2\n");
   printf("--refine_freq - frequency (timesteps) of plotting (0 for none)\n");
   printf("--code - closely minic communication of different codes\n");
   printf("         0 minimal sends, 1 send ghosts, 2 send ghosts and process on send\n");
   printf("--permute - altenates directions in communication\n");
   printf("--num_objects - (>= 0) number of objects to cause refinement\n");
   printf("--object - type, position, movement, size, size rate of change\n");

   printf("All associated settings are integers except for objects\n");
   hpx_print_help();
}

int check_input(int *params)
{
   int error = 0;

  int max_num_blocks = params[ 0];
  int target_active = params[ 1];
  int num_refine = params[ 2];
  int uniform_refine = params[ 3];
  int x_block_size = params[ 4];
  int y_block_size = params[ 5];
  int z_block_size = params[ 6];
  int num_vars = params[ 7];
  int comm_vars = params[ 8];
  int init_block_x = params[ 9];
  int init_block_y = params[10];
  int init_block_z = params[11];
  int reorder = params[12];
  int npx = params[13];
  int npy = params[14];
  int npz = params[15];
  int inbalance = params[16];
  int refine_freq = params[17];
  int report_diffusion = params[18];
  int error_tol = params[19];
  int num_tsteps = params[20];
  int stencil = params[21];
  int report_perf = params[22];
  int plot_freq = params[23];
  int num_objects = params[24];
  int checksum_freq = params[25];
  int target_max = params[26];
  int target_min = params[27];
  int stages_per_ts = params[28];
  int lb_opt = params[29];
  int block_change = params[30];
  int code = params[31];
  int permute = params[32];
  int num_pes = params[33];

   if (init_block_x < 1 || init_block_y < 1 || init_block_z < 1) {
      printf("initial blocks on processor must be positive\n");
      error = 1;
   }
   if (max_num_blocks < init_block_x*init_block_y*init_block_z) {
      printf("max_num_blocks not large enough\n");
      error = 1;
   }
   if (x_block_size < 1 || y_block_size < 1 || z_block_size < 1) {
      printf("block size must be positive\n");
      error = 1;
   }
   if (((x_block_size/2)*2) != x_block_size) {
      printf("block size in x direction must be even\n");
      error = 1;
   }
   if (((y_block_size/2)*2) != y_block_size) {
      printf("block size in y direction must be even\n");
      error = 1;
   }
   if (((z_block_size/2)*2) != z_block_size) {
      printf("block size in z direction must be even\n");
      error = 1;
   }
   if (target_active && target_max) {
      printf("Only one of target_active and target_max can be used\n");
      error = 1;
   }
   if (target_active && target_min) {
      printf("Only one of target_active and target_min can be used\n");
      error = 1;
   }
   if (target_active < 0 || target_active > max_num_blocks) {
      printf("illegal value for target_active\n");
      error = 1;
   }
   if (target_max < 0 || target_max > max_num_blocks ||
       target_max < target_active) {
      printf("illegal value for target_max\n");
      error = 1;
   }
   if (target_min < 0 || target_min > max_num_blocks ||
       target_min > target_active || target_min > target_max) {
      printf("illegal value for target_min\n");
      error = 1;
   }
   if (num_refine < 0) {
      printf("number of refinement levels must be non-negative\n");
      error = 1;
   }
   if (block_change < 0) {
      printf("number of refinement levels must be non-negative\n");
      error = 1;
   }
   if (num_vars < 1) {
      printf("number of variables must be positive\n");
      error = 1;
   }
   if (num_pes != npx*npy*npz) {
      printf("number of processors used does not match number allocated\n");
      error = 1;
   }
if (stencil != 7 && stencil != 27) {
      printf("illegal value for stencil\n");
      error = 1;
   }
   if (stencil == 27 && num_refine && !uniform_refine)
      printf("WARNING: 27 point stencil with non-uniform refinement: answers may diverge\n");
   if (comm_vars == 0 || comm_vars > num_vars)
      comm_vars = num_vars;
   if (code < 0 || code > 2) {
      printf("code must be 0, 1, or 2\n");
      error = 1;
   }
   if (lb_opt < 0 || lb_opt > 2) {
      printf("lb_opt must be 0, 1, or 2\n");
      error = 1;
   }

   return (error);
}



int main(int argc, char **argv)
{
  // default
  int num_pes = 8;
  int params[34];

  int max_num_blocks = 500;
  int target_active = 0;
  int target_max = 0;
  int target_min = 0;
  int num_refine = 5;
  int uniform_refine = 0;
  int x_block_size = 10;
  int y_block_size = 10;
  int z_block_size = 10;
  int num_vars = 40;
  int comm_vars = 0;
  int init_block_x = 1;
  int init_block_y = 1;
  int init_block_z = 1;
  int reorder = 1;
  int npx = 2;
  int npy = 2;
  int npz = 2;
  int inbalance = 0;
  int refine_freq = 5;
  int report_diffusion = 0;
  int error_tol = 8;
  int num_tsteps = 20;
  int stages_per_ts = 20;
  int checksum_freq = 5;
  int stencil = 7;
  int report_perf = 1;
  int plot_freq = 1;
  int num_objects = 0;
  int lb_opt = 1;
  int block_change = 0;
  int code = 0;
  int permute = 0;
  int object_num = 0;

  RunArgs *runargs = NULL;
  
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int i;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--num_pes"))
      num_pes = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--max_blocks"))
      max_num_blocks = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--target_active"))
      target_active = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--target_max"))
       target_max = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--target_min"))
       target_min = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--num_refine"))
       num_refine = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--block_change"))
       block_change = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--uniform_refine"))
       uniform_refine = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--nx"))
       x_block_size = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--ny"))
       y_block_size = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--nz"))
       z_block_size = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--num_vars"))
       num_vars = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--comm_vars"))
       comm_vars = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--init_x"))
       init_block_x = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--init_y"))
       init_block_y = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--init_z"))
       init_block_z = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--reorder"))
       reorder = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--npx"))
       npx = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--npy"))
       npy = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--npz"))
       npz = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--inbalance"))
       inbalance = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--lb_opt"))
       lb_opt = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--refine_freq"))
       refine_freq = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--report_diffusion"))
       report_diffusion = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--error_tol"))
       error_tol = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--num_tsteps"))
       num_tsteps = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--stages_per_ts"))
       stages_per_ts = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--checksum_freq"))
       checksum_freq = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--stencil"))
       stencil = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--permute"))
       permute = 1;
    else if (!strcmp(argv[i], "--report_perf"))
       report_perf = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--plot_freq"))
       plot_freq = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--code"))
       code = atoi(argv[++i]);
    else if (!strcmp(argv[i], "--num_objects")) {
       num_objects = atoi(argv[++i]);
       runargs =  malloc(sizeof(RunArgs) + (sizeof(object) * num_objects));
       object_num = 0;
    } else if (!strcmp(argv[i], "--object")) {
       if (runargs == NULL) {
         printf("please specify --num_objects before --object on the command "
                "line\n");
         exit(-1);
       }
       if (object_num >= num_objects) {
          printf("object number greater than num_objects\n");
          exit(-1);
       }
       runargs->objects[object_num].type = atoi(argv[++i]);
       runargs->objects[object_num].bounce = atoi(argv[++i]);
       runargs->objects[object_num].cen[0] = atof(argv[++i]);
       runargs->objects[object_num].cen[1] = atof(argv[++i]);
       runargs->objects[object_num].cen[2] = atof(argv[++i]);
       runargs->objects[object_num].move[0] = atof(argv[++i]);
       runargs->objects[object_num].move[1] = atof(argv[++i]);
       runargs->objects[object_num].move[2] = atof(argv[++i]);
       runargs->objects[object_num].size[0] = atof(argv[++i]);
       runargs->objects[object_num].size[1] = atof(argv[++i]);
       runargs->objects[object_num].size[2] = atof(argv[++i]);
       runargs->objects[object_num].inc[0] = atof(argv[++i]);
       runargs->objects[object_num].inc[1] = atof(argv[++i]);
       runargs->objects[object_num].inc[2] = atof(argv[++i]);
       object_num++;
    } else if (!strcmp(argv[i], "--help")) {
       usage(stdout);
       return 1;
    } else {
       printf("** Error ** Unknown input parameter %s\n", argv[i]);
       usage(stdout);
       return 1;
    }
  }

  if (!block_change)
     block_change = num_refine;

  params[ 0] = max_num_blocks;
  params[ 1] = target_active;
  params[ 2] = num_refine;
  params[ 3] = uniform_refine;
  params[ 4] = x_block_size;
  params[ 5] = y_block_size;
  params[ 6] = z_block_size;
  params[ 7] = num_vars;
  params[ 8] = comm_vars;
  params[ 9] = init_block_x;
  params[10] = init_block_y;
  params[11] = init_block_z;
  params[12] = reorder;
  params[13] = npx;
  params[14] = npy;
  params[15] = npz;
  params[16] = inbalance;
  params[17] = refine_freq;
  params[18] = report_diffusion;
  params[19] = error_tol;
  params[20] = num_tsteps;
  params[21] = stencil;
  params[22] = report_perf;
  params[23] = plot_freq;
  params[24] = num_objects;
  params[25] = checksum_freq;
  params[26] = target_max;
  params[27] = target_min;
  params[28] = stages_per_ts;
  params[29] = lb_opt;
  params[30] = block_change;
  params[31] = code;
  params[32] = permute;
  params[33] = num_pes;

  if (check_input(params)) {
     printf("** Error ** input problem\n");
     return 1;
  }

  // runargs wasn't allocated if there was no --num_objects on the command line
  if (!runargs)
    runargs = malloc(sizeof(*runargs));

  for (object_num = 0; object_num < num_objects; object_num++)
      for (i = 0; i < 3; i++) {
         runargs->objects[object_num].orig_cen[i] = runargs->objects[object_num].cen[i];
         runargs->objects[object_num].orig_move[i] = runargs->objects[object_num].move[i];
         runargs->objects[object_num].orig_size[i] = runargs->objects[object_num].size[i];
      }

  HPX_REGISTER_ACTION(&_main, _main_action);
  HPX_REGISTER_ACTION(&_initDomain, _initDomain_action);
  HPX_REGISTER_ACTION(&_advance, _advance_action);
  HPX_REGISTER_ACTION(&_plot_sends, _plot_sends_action);
  HPX_REGISTER_ACTION(&_plot_result, _plot_result_action);
  HPX_REGISTER_ACTION(&_comm_refine_sends, _comm_refine_sends_action);
  HPX_REGISTER_ACTION(&_comm_refine_result, _comm_refine_result_action);
  HPX_REGISTER_ACTION(&_comm_parent_sends, _comm_parent_sends_action);
  HPX_REGISTER_ACTION(&_comm_parent_result, _comm_parent_result_action);
  HPX_REGISTER_ACTION(&_comm_parent_reverse_sends,
                      _comm_parent_reverse_sends_action);
  HPX_REGISTER_ACTION(&_comm_parent_reverse_result,
                      _comm_parent_reverse_result_action);
  HPX_REGISTER_ACTION(&_comm_reverse_refine_sends,
                      _comm_reverse_refine_sends_action);
  HPX_REGISTER_ACTION(&_comm_reverse_refine_result,
                      _comm_reverse_refine_result_action);
  HPX_REGISTER_ACTION(&_comm_proc_sends, _comm_proc_sends_action);
  HPX_REGISTER_ACTION(&_comm_proc_result, _comm_proc_result_action);
  HPX_REGISTER_ACTION(&_comm_parent_proc_sends,
                      _comm_parent_proc_sends_action);
  HPX_REGISTER_ACTION(&_comm_parent_proc_result,
                      _comm_parent_proc_result_action);

  printf(" Number of domains: %d\n",num_pes);

  runargs->params = params;
  runargs->paramsize = 34;
  runargs->objectsize = num_objects;
  return hpx_run(&_main, runargs, RunArgs_size(runargs));
}


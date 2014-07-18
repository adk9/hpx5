#include "main.h"

static hpx_action_t _main          = 0;

static void initdouble(double *input, const size_t size) {
  assert(sizeof(double) == size);
  *input = 99999999.0;
}

static void mindouble(double *output,const double *input, const size_t size) {
  assert(sizeof(double) == size);
  if ( *output > *input ) *output = *input;
  return;
}

static int _main_action(int *input)
{
  hpx_time_t t1 = hpx_time_now();

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);

  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
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
  hpx_config_t cfg = {
    .cores         = 0,
    .threads       = 0,
    .stack_bytes   = 0,
    .gas           = HPX_GAS_PGAS
  };

  // default
  int num_pes = 8;
  cfg.threads = 8;
  cfg.cores = 8;
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
  int plot_freq = 0;
  int num_objects = 0;
  int lb_opt = 1;
  int block_change = 0;
  int code = 0;
  int permute = 0;
  int object_num = 0;
  object *objects;

  int i;
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-c")) 
      cfg.cores = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-t" ))
      cfg.threads = atoi(argv[++i]);
    else if (!strcmp(argv[i], "-D" )) {
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
    } else if (!strcmp(argv[i], "-d" )) {
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(argv[++i]);
    } else if (!strcmp(argv[i], "--num_pes"))
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
       objects = (object *) malloc(num_objects*sizeof(object));
       object_num = 0;
    } else if (!strcmp(argv[i], "--object")) {
       if (object_num >= num_objects) {
          printf("object number greater than num_objects\n");
          exit(-1);
       }
       objects[object_num].type = atoi(argv[++i]);
       objects[object_num].bounce = atoi(argv[++i]);
       objects[object_num].cen[0] = atof(argv[++i]);
       objects[object_num].cen[1] = atof(argv[++i]);
       objects[object_num].cen[2] = atof(argv[++i]);
       objects[object_num].move[0] = atof(argv[++i]);
       objects[object_num].move[1] = atof(argv[++i]);
       objects[object_num].move[2] = atof(argv[++i]);
       objects[object_num].size[0] = atof(argv[++i]);
       objects[object_num].size[1] = atof(argv[++i]);
       objects[object_num].size[2] = atof(argv[++i]);
       objects[object_num].inc[0] = atof(argv[++i]);
       objects[object_num].inc[1] = atof(argv[++i]);
       objects[object_num].inc[2] = atof(argv[++i]);
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

  for (object_num = 0; object_num < num_objects; object_num++)
      for (i = 0; i < 3; i++) {
         objects[object_num].orig_cen[i] = objects[object_num].cen[i];
         objects[object_num].orig_move[i] = objects[object_num].move[i];
         objects[object_num].orig_size[i] = objects[object_num].size[i];
      }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  _main      = HPX_REGISTER_ACTION(_main_action);

  printf(" Number of domains: %d cores: %d threads: %d\n",num_pes,cfg.cores,cfg.threads);

  return hpx_run(_main, params, 34*sizeof(int));

  return 0;
}


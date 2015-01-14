#include "mg.h"

static hpx_action_t _main          = 0;

static int _main_action(int *input)
{
  hpx_time_t tick = hpx_time_now();
  printf(" Tick: %g\n", hpx_time_us(tick));

  hpx_time_t t1 = hpx_time_now();

  double elapsed = hpx_time_elapsed_ms(t1);
  printf(" Elapsed: %g\n",elapsed);
  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-h, show help\n");
  hpx_print_help();
  fflush(f);
}


int main(int argc, char **argv)
{

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
      case 'h':
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  // Parameters
  int scaling = 2; // 1 strong scaling, 2 weak scaling
  int comm_method = 10;  // 10 bspma, 11 svaf
  int stencil = 21; // 20 none, 21 2d5pt, 22 2d9pt,23 3d7pt,24 3d27pt
  int bc = 31; // 31 -dirichlet
  int cp_method = 1; // 1 -- mpiio, 2 -- h5part
  int nx = 100;
  int ny = 100;
  int nz = 100;
  int nvars = 5;
  int ntsteps = 100;
  int nspikes = 1;
  int percent_sum = 0;
  int debug_grid = 0; // 1: zero domain, insert heat source (spike) into center
  int report_diffusion = 0;
  int report_perf = 0;
  int cp_interval = 0;
  int restart_cp_num = 0;

  HPX_REGISTER_ACTION(_main_action, &_main);

  int input[17];
  input[0] = scaling;
  input[1] = comm_method;
  input[2] = stencil;
  input[3] = bc;
  input[4] = cp_method;
  input[5] = nx;
  input[6] = ny;
  input[7] = nz;
  input[8] = nvars;
  input[9] = ntsteps;
  input[10] = nspikes;
  input[11] = percent_sum;
  input[12] = debug_grid;
  input[13] = report_diffusion;
  input[14] = report_perf;
  input[15] = cp_interval;
  input[16] = restart_cp_num;

  return hpx_run(&_main, input, 17*sizeof(int));
}


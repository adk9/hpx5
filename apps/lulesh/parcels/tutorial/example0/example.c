#include "example.h"

static hpx_action_t _main          = 0;

static int _main_action(int *input)
{

  hpx_shutdown(0);
}

static void usage(FILE *f) {
  fprintf(f, "Usage: [options]\n"
          "\t-c, cores\n"
          "\t-t, scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-n, number of domains,nDoms\n"
          "\t-i, maxcycles\n"
          "\t-h, show help\n");
}


int main(int argc, char **argv)
{
  hpx_config_t cfg = {
    .cores         = 0,
    .threads       = 0,
    .stack_bytes   = 0,
    .gas           = HPX_GAS_PGAS
  };

  int nDoms, nx, maxcycles,cores;
  // default
  nDoms = 8;
  maxcycles = 10;
  cores = 8;
  cfg.cores = cores;

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:D:n:ih")) != -1) {
    switch (opt) {
      case 'c':
        cfg.cores = atoi(optarg);
        cores = cfg.cores;
        break;
      case 't':
        cfg.threads = atoi(optarg);
        break;
      case 'D':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = HPX_LOCALITY_ALL;
        break;
      case 'd':
        cfg.wait = HPX_WAIT;
        cfg.wait_at = atoi(optarg);
        break;
      case 'n':
        nDoms = atoi(optarg);
        break;
      case 'i':
        maxcycles = atoi(optarg);
        break;
      case 'h':
        usage(stdout);
        return 0;
      case '?':
      default:
        usage(stderr);
        return -1;
    }
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX failed to initialize.\n");
    return 1;
  }

  _main      = HPX_REGISTER_ACTION(_main_action);

  int input[3];
  input[0] = nDoms;
  input[1] = maxcycles;
  input[2] = cores;
  printf(" Number of domains: %d maxcycles: %d cores: %d\n",nDoms,maxcycles,cores);

  return hpx_run(_main, input, 3*sizeof(int));

  return 0;
}


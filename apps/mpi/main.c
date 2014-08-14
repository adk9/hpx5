#include <stdio.h>
#include <unistd.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

static hpx_action_t _mpi = 0;
static hpx_action_t _hpxmain = 0;

void mpi_test_routine(int its)
{
  int numProcs;
  MPI_Comm_size(MPI_COMM_WORLD, &numProcs);
  printf(" Number of procs %d\n",numProcs);
}

static int _mpi_action(int args[1] /* its */) {
  int err;
  mpi_init_(&err);
  mpi_test_routine(args[0]);
  mpi_finalize_(&err);
  return HPX_SUCCESS;
}

static int _hpxmain_action(int args[2] /* hpxranks, its */) {
  mpi_system_init(args[0], 0);

  hpx_addr_t and = hpx_lco_and_new(args[0]);
  int i,j,e;
  for (i = 0, e = hpx_get_num_ranks(); i < e; ++i) {
    int size = args[0];
    int ranks_there = 0; // important to be 0
    get_ranks_per_node(i, &size, &ranks_there, NULL);

    
    for (j = 0; j < ranks_there; ++j)
      hpx_call(HPX_THERE(i), _mpi, &args[1], sizeof(int), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  mpi_system_shutdown();
  hpx_shutdown(0);
}

int main(int argc, char *argv[])
{

   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   printf("PID %d on %s ready for attach\n", getpid(), hostname);
   fflush(stdout);
   //   sleep(8);

  if ( argc < 4 ) {
    printf(" Usage: test <number of OS threads> <# of persistent hpx threads> <# of iterations>\n");
    exit(0);
  }

  uint64_t numos = atoll(argv[1]);
  int numhpx = atoi(argv[2]);
  int its = atoi(argv[3]);

  printf(" Number OS threads: %ld Number persistent lightweight threads: %d its: %d\n",numos,numhpx,its);

  hpx_config_t cfg = {
    .cores = numos,
    .threads = numos,
    //    .stack_bytes = 2<<24
    .gas = HPX_GAS_PGAS
  };

  int error = hpx_init(&cfg);
  if (error != HPX_SUCCESS)
    exit(-1);

  mpi_system_register_actions();

  // register local actions
  _mpi = HPX_REGISTER_ACTION(_mpi_action);
  _hpxmain = HPX_REGISTER_ACTION(_hpxmain_action);

  // do stuff here
  int args[2] = { numhpx, its };
  hpx_run(_hpxmain, args, sizeof(args));

  return 0;
}

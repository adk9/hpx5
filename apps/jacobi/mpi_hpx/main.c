#include <stdio.h>
#include <unistd.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

static hpx_action_t _mpi = 0;
static hpx_action_t _hpxmain = 0;
extern hpx_time_t start_time;
hpx_time_t start_time;

void jacobi_();

void mpi_test_routine(int its)
{
   jacobi_();
}

static int _mpi_action(int args[1] /* its */) {
  int err;
  start_time = hpx_time_now();
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
  hpx_shutdown(HPX_SUCCESS);
}

int main(int argc, char *argv[])
{

  int error = hpx_init(&argc, &argv);
  if (error != HPX_SUCCESS)
    exit(-1);
  
  if ( argc < 2 ) {
    printf(" Usage: test <# of persistent hpx threads>\n");
    exit(0);
  }

  int numhpx = atoi(argv[1]);
  int its = 1;

  printf(" Number persistent lightweight threads: %d its: %d\n",numhpx,its);

  mpi_system_register_actions();

  // register local actions
  HPX_REGISTER_ACTION(_mpi_action, &_mpi);
  HPX_REGISTER_ACTION(_hpxmain_action, &_hpxmain);

  // do stuff here
  int args[2] = { numhpx, its };
  hpx_run(&_hpxmain, args, sizeof(args));

  return 0;
}

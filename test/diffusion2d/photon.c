#include <stdlib.h>
#include <assert.h>
#include <mpi.h>
#include <photon.h>

#include "d2dmodule.h"

static int wait = 0;

void module_init() {
  int sizes[2];
  int subsizes[2];
  int starts[2];

  char **forwarders = malloc(sizeof(char**));
  forwarders[0] = "b006/5006";

  struct photon_config_t cfg = {
    .meta_exch = PHOTON_EXCH_MPI,
    .nproc = nprocs,
    .address = rank,
    .comm = MPI_COMM_WORLD,
    .use_forwarder = 1,
    .forwarder_eids = forwarders,
    .use_cma = 1,
    .eth_dev = "roce0",
    .ib_dev = "qib0",
    .ib_port = 1,
    .backend = "verbs"
  };

  photon_init(&cfg);

  iobuf_size = sizeof(double)*nxl*nyl;
  iobuf = malloc(iobuf_size);
  assert(iobuf);

  if (photon_register_buffer((char*)iobuf, iobuf_size) != 0) {
    fprintf(stderr, "Error: couldn't register photon io buffer\n");
    exit(-1);
  }

  sizes[0] = nx;
  sizes[1] = ny;
  subsizes[0] = nxl;
  subsizes[1] = nyl;
  starts[0] = xcoord*nxl;
  starts[1] = ycoord*nyl;

  MPI_Type_create_subarray(2, sizes, subsizes, starts, MPI_ORDER_C,
                           MPI_DOUBLE, &filetype);
  MPI_Type_commit(&filetype);

  photon_io_init(fileuri, MPI_MODE_CREATE | MPI_MODE_WRONLY,
                 filetype, nsteps/wstep+1);
}

void pre_exchange(int io_frame) {
  return;
  if (wait) {
    photon_wait(photon_request);
    MPI_Barrier(MPI_COMM_WORLD);
    wait = 0;
  }
}

/* write_frame: writes current values of u to file */
void write_frame(int time) {
  int i, j;

  if (time)
    photon_wait(photon_request);

  for (i = 0; i < nyl; i++)
    for (j = 0; j < nxl; j++)
      iobuf[i*nxl+j] = u[j+1][i+1];

  photon_wait_recv_buffer_rdma(phorwarder, time/wstep);
  photon_post_os_put(phorwarder, (char*)iobuf, iobuf_size, time/wstep, 0, &photon_request);
  photon_send_FIN(phorwarder);

  ++wait;
}

void module_finalize() {
  if (wait)
    photon_wait(photon_request);
  photon_io_finalize();
  //photon_finalize();
  MPI_Type_free(&filetype);
}

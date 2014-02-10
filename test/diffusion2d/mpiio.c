#include <mpi.h>
#include <assert.h>
#include <stdlib.h>
#include "d2dmodule.h"

static double *buf;

void module_init() {
  int sizes[2];
  int subsizes[2];
  int starts[2];

  buf = malloc(nxl*nyl*sizeof(double));
  assert(buf);

  sizes[0] = nx;
  sizes[1] = ny;
  subsizes[0] = nxl;
  subsizes[1] = nyl;
  starts[0] = xcoord*nxl;
  starts[1] = ycoord*nyl;

  MPI_Type_create_subarray(2, sizes, subsizes, starts, MPI_ORDER_C,
                           MPI_DOUBLE, &filetype);
  MPI_Type_commit(&filetype);

  MPI_File_open(MPI_COMM_WORLD, fileuri, MPI_MODE_CREATE | MPI_MODE_RDWR,
                MPI_INFO_NULL, &filehandle);
  MPI_File_set_view(filehandle, 0, MPI_DOUBLE, filetype, "native", MPI_INFO_NULL);
}

void pre_exchange(int io_frame) {
}

/* write_frame: writes current values of u to file */
void write_frame(int time) {
  int i, j;

  for (i = 0; i < nyl; i++)
    for (j = 0; j < nxl; j++)
      buf[i*nxl+j] = u[j+1][i+1];

  MPI_File_write_all(filehandle, buf, nxl*nyl, MPI_DOUBLE, MPI_STATUS_IGNORE);
}

void module_finalize() {
  MPI_File_close(&filehandle);
  MPI_Type_free(&filetype);
}

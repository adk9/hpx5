#ifndef PHOTON_IO_H
#define PHOTON_IO_H

#include <mpi.h>

typedef enum { PHOTON_CI, PHOTON_RI, PHOTON_SI, PHOTON_FI, PHOTON_IO, PHOTON_MTU } photon_info_t;

int photon_io_init(char *file, int amode, MPI_Datatype view, int niter);
int photon_io_finalize();
void photon_io_print_info(void *io);

#endif

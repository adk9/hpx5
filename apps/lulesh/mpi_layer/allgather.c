#include <stdio.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

void mpi_allgather_(void *sendbuf, int *fsendcounts,
                   MPI_Datatype *fsendtype, void *recvbuf, int *recvcounts,
                   MPI_Datatype *frecvtype, MPI_Comm *fcomm, int* pier) {

  *pier = ERROR;
  MPI_Comm comm = *fcomm;
  int rank;
  int size;
  int typesize;
  int error;
  mpi_comm_rank_(&comm, &rank, &error);
  if (error != MPI_SUCCESS_)
    return;
  mpi_comm_size_(&comm, &size, &error);
  if (error != MPI_SUCCESS_)
    return;

  for (int i = 0; i < size; i++) {
    mpi_gather_(sendbuf,fsendcounts,fsendtype,
                 recvbuf,recvcounts,frecvtype,&i,fcomm,pier);
  }

  *pier = MPI_SUCCESS_;
  return;
}

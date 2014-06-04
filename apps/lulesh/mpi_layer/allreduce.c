#include <stdio.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"


int mpi_allreduce(void *sendbuf, void *recvbuf, int recvcounts, 
                   MPI_Datatype frecvtype, MPI_Op pop, MPI_Comm comm) {
  int size,error;
  mpi_comm_size_(&comm, &size, &error);
  if (error != MPI_SUCCESS_)
    return ERROR;

  int root = size-1;
  int pier;
  mpi_reduce_(sendbuf,recvbuf,&recvcounts,
               &frecvtype,&pop,&root,&comm,&pier);

  mpi_bcast_(recvbuf,&recvcounts,&frecvtype,&root,&comm,&pier);

  return MPI_SUCCESS_;
}

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

  pier = mpi_bcast(recvbuf,recvcounts,frecvtype,root,comm);

  return MPI_SUCCESS_;
}

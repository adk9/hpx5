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

void mpi_allreduce_(void *sendbuf, void *recvbuf, int *frecvcounts, 
                   MPI_Datatype *frecvtype, MPI_Op *fpop, MPI_Comm *fcomm,int *ierr) {

  int recvcounts = *frecvcounts;
  MPI_Datatype recvtype = *frecvtype;
  MPI_Op pop = *fpop;
  MPI_Comm comm = *fcomm;

  *ierr = mpi_allreduce(sendbuf,recvbuf,recvcounts,recvtype,pop,comm);
  return;
}

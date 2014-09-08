#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

int mpi_allgather(void *sendbuf, int sendcount, MPI_Datatype sendtype,
                  void *recvbuf, int recvcount, MPI_Datatype recvtype,
                  MPI_Comm comm)
{
  int ierr;
  int root = 0;

  ierr=mpi_gather(sendbuf,sendcount,sendtype,
                   recvbuf,recvcount, recvtype,
                   root,comm);

  ierr = mpi_bcast(recvbuf,recvcount,recvtype,root,comm);

  return ierr;
}

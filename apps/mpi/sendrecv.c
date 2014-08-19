#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

int mpi_sendrecv(const void *sendbuf, int sendcount, MPI_Datatype sendtype,
                int dest, int sendtag,
                void *recvbuf, int recvcount, MPI_Datatype recvtype,
                int source, int recvtag,
                MPI_Comm comm, MPI_Status *status)
{
  int ierr;
  MPI_Request   send_request,recv_request;
  ierr=mpi_isend(sendbuf,sendcount,sendtype,
                   dest,sendtag,MPI_COMM_WORLD_,&send_request);
  ierr=mpi_irecv(recvbuf,recvcount,recvtype,
                   source,recvtag,MPI_COMM_WORLD_,&recv_request);
  ierr=mpi_wait(&send_request,&status);
  ierr=mpi_wait(&recv_request,&status);
}

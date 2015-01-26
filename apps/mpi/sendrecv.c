#include <hpx/hpx.h>
#include <stdio.h>
#include <string.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

int mpi_sendrecv(void *sendbuf, int sendcount, MPI_Datatype sendtype,
         int dest, int sendtag,
         void *recvbuf, int recvcount, MPI_Datatype recvtype,
         int source, int recvtag,
         MPI_Comm comm, MPI_Status *status)
{
  int ierr = 0;
  MPI_Request send_request,recv_request;
  if ( dest != MPI_PROC_NULL ) {
    ierr = mpi_isend(sendbuf, sendcount, sendtype,
           dest, sendtag, MPI_COMM_WORLD_, &send_request);
  }
  if ( source != MPI_PROC_NULL ) {
    ierr = mpi_irecv(recvbuf, recvcount, recvtype,
           source, recvtag, MPI_COMM_WORLD_, &recv_request);
  }
  if ( dest != MPI_PROC_NULL ) {
    ierr = mpi_wait(&send_request, status);
  }
  if ( source != MPI_PROC_NULL ) {
    ierr = mpi_wait(&recv_request, status);
  }
  return ierr;
}

void mpi_sendrecv_(void *sendbuf, int *fsendcount, MPI_Datatype *fsendtype,
           int *fdest, int *fsendtag,
           void *recvbuf, int *frecvcount, MPI_Datatype *frecvtype,
           int *fsource, int *frecvtag,
           MPI_Comm *fcomm, MPI_Status *status, int *ierr)
{
  int sendcount = *fsendcount;
  MPI_Datatype sendtype = *fsendtype;
  int dest = *fdest;
  int sendtag = *fsendtag;
  int recvcount = *frecvcount;
  MPI_Datatype recvtype = *frecvtype;
  int source = *fsource;
  int recvtag = *frecvtag;
  MPI_Comm comm = *fcomm;
  *ierr = mpi_sendrecv(sendbuf, sendcount, sendtype, dest, sendtag,
                       recvbuf, recvcount, recvtype, source, recvtag, comm, status);
  return;
}

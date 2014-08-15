#ifndef MPI_WRAPPER_H
#define MPI_WRAPPER_H

#include "logging.h"

#ifndef USE_REAL_MPI

/////////////////////////////////////////////////////////////////////////////////
// MPI datatypes ////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

typedef int MPI_Datatype;
#define MPI_CHAR           ((MPI_Datatype)1)
#define MPI_UNSIGNED_CHAR  ((MPI_Datatype)2)
#define MPI_BYTE           ((MPI_Datatype)3)
#define MPI_SHORT          ((MPI_Datatype)4)
#define MPI_UNSIGNED_SHORT ((MPI_Datatype)5)
#define MPI_INT            ((MPI_Datatype)6)
#define MPI_UNSIGNED       ((MPI_Datatype)7)
#define MPI_LONG           ((MPI_Datatype)8)
#define MPI_UNSIGNED_LONG  ((MPI_Datatype)9)
#define MPI_FLOAT          ((MPI_Datatype)10)
#define MPI_DOUBLE         ((MPI_Datatype)11)
#define MPI_LONG_DOUBLE    ((MPI_Datatype)12)
#define MPI_LONG_LONG_INT  ((MPI_Datatype)13)

#define MPI_PACKED         ((MPI_Datatype)14)
#define MPI_LB             ((MPI_Datatype)15)
#define MPI_UB             ((MPI_Datatype)16)
#define MPI_FLOAT_INT      ((MPI_Datatype)17)
#define MPI_DOUBLE_INT     ((MPI_Datatype)18)
#define MPI_LONG_INT       ((MPI_Datatype)19)
#define MPI_SHORT_INT      ((MPI_Datatype)20)
#define MPI_2INT           ((MPI_Datatype)21)
#define MPI_LONG_DOUBLE_INT ((MPI_Datatype)22)

/* Fortran types */
#define MPI_COMPLEX        ((MPI_Datatype)23)
#define MPI_DOUBLE_COMPLEX ((MPI_Datatype)24)
#define MPI_LOGICAL        ((MPI_Datatype)25)
#define MPI_REAL           ((MPI_Datatype)26)
#define MPI_DOUBLE_PRECISION ((MPI_Datatype)27)
#define MPI_INTEGER        ((MPI_Datatype)28)
#define MPI_2INTEGER       ((MPI_Datatype)29)
#define MPI_2COMPLEX       ((MPI_Datatype)30)
#define MPI_2DOUBLE_COMPLEX   ((MPI_Datatype)31)
#define MPI_2REAL             ((MPI_Datatype)32)
#define MPI_2DOUBLE_PRECISION ((MPI_Datatype)33)
#define MPI_CHARACTER         ((MPI_Datatype)1)

///////////////////////////////////////////////

typedef int MPI_Comm;
#define MPI_COMM_WORLD_ 0
#define MPI_SUCCESS_ SUCCESS
#define MPI_REQUEST_NULL 0

#define MPI_ANY_TAG_ -1
#define MPI_ANY_SOURCE_ -1


typedef int MPI_Request;
typedef int MPI_Status;

typedef int MPI_Op;
#define MPI_MAX    (MPI_Op)(100)
#define MPI_MIN    (MPI_Op)(101)
#define MPI_SUM    (MPI_Op)(102)
#define MPI_PROD   (MPI_Op)(103)
#define MPI_LAND   (MPI_Op)(104)
#define MPI_BAND   (MPI_Op)(105)
#define MPI_LOR    (MPI_Op)(106)
#define MPI_BOR    (MPI_Op)(107)
#define MPI_LXOR   (MPI_Op)(108)
#define MPI_BXOR   (MPI_Op)(109)
#define MPI_MINLOC (MPI_Op)(110)
#define MPI_MAXLOC (MPI_Op)(111)

int mpi_isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request_num);
int mpi_irecv(void *buf, int count, MPI_Datatype datatype,
	      int source, int tag, MPI_Comm comm, MPI_Request *request_num);
int mpi_send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm);
int mpi_recv(void *buf, int count, MPI_Datatype datatype,
	      int source, int tag, MPI_Comm comm, MPI_Status *status);
int mpi_wait(MPI_Request *request_num, MPI_Status *status);
void mpi_waitall(int pcount,MPI_Request *request_num, MPI_Status *status);

int mpi_allreduce(void *sendbuf, void *recvbuf, int recvcounts, 
		  MPI_Datatype frecvtype, MPI_Op pop, MPI_Comm comm);
void mpi_reduce(void *sendbuf, void *recvbuf, int recvcounts, 
		MPI_Datatype frecvtype, MPI_Op pop, int froot, MPI_Comm fcomm);


void mpi_comm_rank(int comm, int* rank);
void mpi_comm_size(int *comm, int*size);

int mpi_init(int *argc, char*** argv);
int mpi_finalize();

int mpi_gather(void *sendbuf, int sendcounts,
                   MPI_Datatype sendtype, void *recvbuf, int recvcounts,
                   MPI_Datatype recvtype, int root, MPI_Comm comm);


#define MPI_Comm_rank mpi_comm_rank
#define MPI_Comm_size mpi_comm_size
#define MPI_Init mpi_init
#define MPI_Finalize mpi_finalize
#define MPI_Irecv mpi_irecv
#define MPI_Isend mpi_isend
#define MPI_Send mpi_send
#define MPI_Recv mpi_recv
#define MPI_Wait mpi_wait
#define MPI_Waitall mpi_waitall
#define MPI_Allreduce mpi_allreduce
#define MPI_Reduce mpi_reduce
#define MPI_Gather mpi_gather
#define MPI_Wtime mpi_wtime
#define MPI_COMM_WORLD MPI_COMM_WORLD_

#else // USE_REAL_MPI

#include <mpi.h>

int MPI_Init(int *argc, char ***argv) {
  int success;
  success = PMPI_Init(argc, argv);
  log_p2p_init();
  return success;
}

int MPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest,
	      int tag, MPI_Comm comm, MPI_Request *request) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);
  struct p2p_record record = log_p2p_start(E_ISEND, rank, dest);
  success = PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
  log_p2p_end(record);
  return success;
}

int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest,
	     int tag, MPI_Comm comm) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);
  struct p2p_record record = log_p2p_start(E_SEND, rank, dest);
  success = PMPI_Send(buf, count, datatype, dest, tag, comm);
  log_p2p_end(record);
  return success;
}


int MPI_Irecv(void *buf, int count, MPI_Datatype datatype,
	      int source, int tag, MPI_Comm comm, MPI_Request *request) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);
  struct p2p_record record = log_p2p_start(E_IRECV, source, rank);
  success = PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
  log_p2p_end(record);
  return success;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype,
	     int source, int tag, MPI_Comm comm, MPI_Status *status) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);
  struct p2p_record record = log_p2p_start(E_RECV, source, rank);
  success = PMPI_Recv(buf, count, datatype, source, tag, comm, status);
  log_p2p_end(record);
  return success;
}

int MPI_Wait(MPI_Request *request, MPI_Status *status) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);
  struct p2p_record record = log_p2p_start(E_WAIT, rank, dest);
  int src;
  if (status != MPI_STATUS_IGNORE) {
    success = PMPI_Wait(request, &our_status);
    src = status.MPI_SOURCE;
  }
  else {
    MPI_Status our_status;
    success = PMPI_Wait(request, &our_status);
    src = our_status.MPI_SOURCE;
  }
  log_p2p_end(record, src);

  return success;
}

int MPI_Waitall(int count, MPI_Request *array_of_requests,
		MPI_Status *array_of_statuses) {
  int success;
  int rank;
  MPI_Comm_rank(comm, &rank);

  struct p2p_record *records = malloc(sizeof(records[0]) * count);
  MPI_Statuses *statuses = malloc(sizeof(statuses[0]) * count);

  for (int i = 0; i < count; i++)
    records[i] = log_p2p_start(E_WAITALL, rank, dest);

  success = PMPI_Waitall(count, array_of_requests, statuses);

  for (int i = 0; i < count; i++) {
    int src = statuses[i].MPI_SOURCE;
    log_p2p_end(records[i], src);
    if (array_of_statuses[i] != MPI_STATUS_IGNORE)
      array_of_statuses[i] = statuses[i];
    log_p2p_end(records[i], src);
  }

  free(records);
  free(statuses);

  return success;
}

int MPI_Finalize() {
  p2p_log_fini();
  return PMPI_Finalize;
}

#endif // USE_REAL_MPI

#endif

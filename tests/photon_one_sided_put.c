#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 32
#define PHOTON_TAG       13
#define ARRAY_SIZE       4
//****************************************************************************
// This testcase tests RDMA one-sided photon_post_os_put_direct functions
//****************************************************************************
START_TEST(test_rdma_one_sided_put_direct) 
{
  struct photon_buffer_t rbuf;
  char *send, *recv;
  int rank, size, next, prev;
  fprintf(detailed_log, "Starting RDMA one sided put direct test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
  }

  photon_rid sendReq, recvReq, directReq;
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  photon_get_buffer_remote(sendReq, &rbuf);
  photon_post_os_put_direct(prev, send, PHOTON_SEND_SIZE, &rbuf, 0, &directReq);

  while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(directReq, &flag, &type, &stat);
    if( tst < 0 ) {
      fprintf(detailed_log,"%d: An error occured in photon_test(recv)\n", rank);
      exit(-1);
    }
    else if( tst > 0 ) {
      fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
      exit(0);
    }
    else {
      if (flag > 0) {
        fprintf(detailed_log,"%d: put(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
        photon_send_FIN(sendReq, prev, PHOTON_REQ_COMPLETED);
        break;
      }
    }
  }

  // clear the FIN event to avoid it being reaped in later tests
  int ret_proc;
  photon_rid req;
  photon_wait_any(&ret_proc, &req);
  photon_wait(recvReq);

  MPI_Barrier(MPI_COMM_WORLD);

  fprintf(detailed_log, "%d received buffer: ", rank);
  int j;
  for (j = 0; j < PHOTON_SEND_SIZE; j++) {
    fprintf(detailed_log, "%d", recv[j]);
  }
  fprintf(detailed_log, "\n");

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);
}
END_TEST

//****************************************************************************
// This testcase tests RDMA one-sided photon_post_os_put_direct functions
//****************************************************************************
START_TEST (test_rdma_one_sided_put_direct_array)
{
  struct photon_buffer_t rbuf[ARRAY_SIZE];

  photon_rid recvReq[ARRAY_SIZE], sendReq[ARRAY_SIZE], request;
  char *send[ARRAY_SIZE], *recv[ARRAY_SIZE];
  int rank, size, prev, next, i, j, ret_proc;

  fprintf(detailed_log, "Starting the photon vectored RDMA one sided put test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  for (i=0; i < ARRAY_SIZE; i++) {
    send[i] = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
    recv[i] = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
    photon_register_buffer(send[i], PHOTON_SEND_SIZE);
    photon_register_buffer(recv[i], PHOTON_SEND_SIZE);

    for (j=0; j<PHOTON_SEND_SIZE; j++) {
      send[i][j] = rand() % 26 + 97;
    }
    send[i][PHOTON_SEND_SIZE] = '\0';

    fprintf(detailed_log, "%d send buf[%d]: %s\n", rank, i, send[i]);
  }

  fflush(stdout);
  
  // everyone posts their send buffers to their next rank
  for (i = 0; i < ARRAY_SIZE; i++) {
    photon_post_recv_buffer_rdma(next, recv[i], PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq[i]);
    photon_wait_any(&ret_proc, &request);
    photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq[i]);
    photon_get_buffer_remote(sendReq[i], &rbuf[i]);
    photon_post_os_put_direct(prev, send[i], PHOTON_SEND_SIZE, &rbuf[i], 0, &sendReq[i]);
  }

  for (i = 0; i < ARRAY_SIZE; i++) {
    while (1) {
      int flag, type;
      struct photon_status_t stat;
      int tst = photon_test(sendReq[i], &flag, &type, &stat);
      if( tst < 0 ) {
	fprintf(detailed_log,"%d: An error occured in photon_test(recv)\n", rank);
	exit(-1);
      }
      else if( tst > 0 ) {
	fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
	exit(0);
      }
      else {
	if (flag > 0) {
	  fprintf(detailed_log,"%d: put(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
	  photon_send_FIN(sendReq[i], prev, PHOTON_REQ_COMPLETED);
	  break;
	}
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  for (i = 0; i < ARRAY_SIZE; i++) {
    recv[i][PHOTON_SEND_SIZE] = '\0';
    fprintf(detailed_log, "%d recv buf[%d]: %s\n", rank, i, recv[i]);
    fflush(stdout);
    photon_unregister_buffer(send[i], PHOTON_SEND_SIZE);
    photon_unregister_buffer(recv[i], PHOTON_SEND_SIZE);
    free(send[i]);
    free(recv[i]);
  }
}
END_TEST

void add_photon_rdma_one_sided_put(TCase *tc) {
  tcase_add_test(tc, test_rdma_one_sided_put_direct);
  tcase_add_test(tc, test_rdma_one_sided_put_direct_array);
}

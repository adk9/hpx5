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
//****************************************************************************
// This unit testcase tests photon handshake functions:
// 1. photon_post_send_request_rdma
// 2. photon_wait_send_request_rdma
//****************************************************************************
START_TEST (test_photon_send_request)
{
  photon_rid recvReq,sendReq;
  char *send,*recv;
  int rank,size,prev,next;

  fprintf(detailed_log, "Starting the RDMA rendezvous send reqest and wait request test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  posix_memalign((void **) &send, 32, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 32, PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
  }

  // post send request RDMA to the next rank
  //photon_post_send_request_rdma(next, PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq);
  // Wait for the send buffer request that was posted from the prev rank
  //photon_wait_send_request_rdma(PHOTON_TAG);

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  // put directly into that recv buffer
  photon_post_os_put(sendReq, prev, send, PHOTON_SEND_SIZE, PHOTON_TAG, 0);

 while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(sendReq, &flag, &type, &stat);
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
        fprintf(detailed_log,"%d: put(%d, %d) of size %d completed successfully\n", rank,
		(int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
	photon_send_FIN(sendReq, prev, 0);
        break;
      }
    }
  }

  while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(recvReq, &flag, &type, &stat);
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
        fprintf(detailed_log,"%d: recv(%d, %d) of size %d completed successfully\n", rank,
		(int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
        break;
      }
    }
  }

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

void add_photon_send_request_test(TCase *tc) {
  tcase_add_test(tc, test_photon_send_request);
}

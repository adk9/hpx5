#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_RECV_SIZE 1024*1024*1024 // 1GB                                                
#define PHOTON_SEND_SIZE 68
#define PHOTON_TAG       13
#define NUM_REQ          9

//****************************************************************************
// Photon test interleaved
//****************************************************************************
START_TEST (test_photon_interleaved)
{
  photon_rid recvReq[NUM_REQ], sendReq[NUM_REQ];
  char *send[NUM_REQ], *recv[NUM_REQ];
  int rank,size,prev,next,i,j;
  fprintf(detailed_log, "Starting the photon interleaved test\n");
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  for (i=0; i<NUM_REQ; i++) {
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

  // everyone posts their send buffers to their next rank
  for (i=0; i<NUM_REQ; i++) {
    photon_post_send_buffer_rdma(next, send[i], PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq[i]);
  }

  // do some "work"
  //sleep(1);

  for (i=0; i<NUM_REQ; i+=3) {
    // wait for the send buffer that was posted from the previous rank
    photon_wait_send_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &recvReq[i]);
    photon_wait_send_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &recvReq[i+1]);
    photon_post_os_get(recvReq[i+1], prev, recv[i+1], PHOTON_SEND_SIZE, PHOTON_TAG, 0);
    photon_wait_send_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &recvReq[i+2]);
    photon_post_os_get(recvReq[i], prev, recv[i], PHOTON_SEND_SIZE, PHOTON_TAG, 0);
    photon_post_os_get(recvReq[i+2], prev, recv[i+2], PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  }

  /* check that all the os_gets completed */
  for (i=0; i<NUM_REQ; i++) {
    while(1) {
      int flag, type;
      struct photon_status_t stat;
      int tst = photon_test(recvReq[i], &flag, &type, &stat);
      if( tst < 0 ) {
        fprintf(detailed_log,"%d: An error occured in photon_test(recv)\n", rank);
        exit(-1);
      }
      else if( tst > 0 ) {
        fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
        exit(0);
      }
      else {
        if( flag ) {
          fprintf(detailed_log,"%d: get(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
          break;
        }
      }
    }
    photon_send_FIN(recvReq[i], prev, 0);
  }
 
  /* check that all the buffers we posted were retrieved */
  for (i=0; i<NUM_REQ; i++) {
    while(1) {
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
        if( flag ) {
          fprintf(detailed_log,"%d: post_send_buf(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
          break;
        }
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  for (i=0; i<NUM_REQ; i++) {
    recv[i][PHOTON_SEND_SIZE] = '\0';
    fprintf(detailed_log, "%d recv buf[%d]: %s\n", rank, i, recv[i]);
    photon_unregister_buffer(send[i], PHOTON_SEND_SIZE);
    photon_unregister_buffer(recv[i], PHOTON_SEND_SIZE);
    free(send[i]);
    free(recv[i]);
  }

}
END_TEST
//****************************************************************************
// Register the testcase photon_message_passing.c
//****************************************************************************
void add_photon_message_passing(TCase *tc) {
  tcase_add_test(tc, test_photon_interleaved);
}

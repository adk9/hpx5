#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 16777216 // 16MB
#define PHOTON_TAG       13

#define PHOTON_LEDGER_SIZE 60     // we can make this larger if needed
#define PHOTON_BUF_SIZE (1024*64) // 64k
#define PHOTON_WC_TAG UINT32_MAX

int send_comp = 0;
int recv_comp = 0;
int myrank, size, rank;

//****************************************************************************
// photon get test
//****************************************************************************
START_TEST (test_photon_test_get) 
{
  photon_rid recvReq, sendReq;
  char *send, *recv;
  int rank, size, prev, next;
  fprintf(detailed_log, "Starting the photon get test\n");


  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;
  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  // everyone posts their send buffer to their next rank
  photon_post_send_buffer_rdma(next, send, PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq);
  // wait for the send buffer that was posted from the previous rank
  photon_wait_send_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &recvReq);
  // get that posted send buffer
  photon_post_os_get(recvReq, prev, recv, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  photon_send_FIN(recvReq, prev, 0);

  while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(sendReq, &flag, &type, &stat);
    if( tst < 0 ) {
      fprintf(detailed_log, "%d: An error occured in photon_test(recv)\n", rank);
      exit(-1);
    }
    else if( tst > 0 ) {
      fprintf(detailed_log, "%d: That shouldn't have happened in this code\n", rank);
      exit(0);
    }
    else {
      if(flag > 0) {
        fprintf(detailed_log,"%d: post_send_buf(%d, %d) of size %lu completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, stat.size);
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
      if(flag > 0) {
        fprintf(detailed_log,"%d: get(%d, %d) of size %lu completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, stat.size);
        break;
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);
}
END_TEST

//****************************************************************************
// photon put test
//****************************************************************************
START_TEST (test_photon_test_put) 
{
  fprintf(detailed_log, "Starting the photon put test\n");
  photon_rid recvReq, sendReq;
  char *send, *recv;
  int rank, size, prev, next;

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  posix_memalign((void **) &send, 64, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 64, PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  // put directly into that recv buffer
  photon_post_os_put(sendReq, prev, send, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  photon_send_FIN(sendReq, prev, 0);

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
      if(flag > 0) {
        fprintf(detailed_log,"%d: put(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
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
      if(flag > 0) {
        fprintf(detailed_log,"%d: recv(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
        break;
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);
}
END_TEST


//****************************************************************************
// Register the testcase photon_data_movement.c
//****************************************************************************
void add_photon_data_movement(TCase *tc) {
  tcase_add_test(tc, test_photon_test_get);
  tcase_add_test(tc, test_photon_test_put);
}

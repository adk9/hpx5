#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "photon.h"

#define PHOTON_SEND_SIZE 16777216 // 16MB
#define PHOTON_TAG       13

#define NUM_REQ          9

int main(int argc, char *argv[]) {
  uint32_t recvReq[NUM_REQ], sendReq[NUM_REQ];
  char *send[NUM_REQ], *recv[NUM_REQ];
  int rank,size,prev,next,i;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  struct photon_config_t cfg = {
    .meta_exch = PHOTON_EXCH_MPI,
    .nproc = size,
    .address = rank,
    .comm = MPI_COMM_WORLD,
    .use_forwarder = 0,
    .use_cma = 1,
    .eth_dev = "roce0",
    .ib_dev = "qib0",
    .ib_port = 1,
    .backend = "verbs"
  };

  photon_init(&cfg);
  
  for (i=0; i<NUM_REQ; i++) {
    send[i] = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
    recv[i] = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
    photon_register_buffer(send[i], PHOTON_SEND_SIZE);
    photon_register_buffer(recv[i], PHOTON_SEND_SIZE);
  }

  // everyone posts their send buffers to their next rank
  for (i=0; i<NUM_REQ; i++) {
    photon_post_send_buffer_rdma(next, send[i], PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq[i]);
  }
  
  // do some "work"
  //sleep(1);

  for (i=0; i<NUM_REQ; i+=3) {
    // wait for the send buffer that was posted from the previous rank
    photon_wait_send_buffer_rdma(prev, PHOTON_TAG, &recvReq[i]);
    photon_wait_send_buffer_rdma(prev, PHOTON_TAG, &recvReq[i+1]);

    photon_post_os_get(recvReq[i+1], prev, recv[i+1], PHOTON_SEND_SIZE, PHOTON_TAG, 0);
    photon_send_FIN(recvReq[i+1], prev);
    
    photon_wait_send_buffer_rdma(prev, PHOTON_TAG, &recvReq[i+2]);    

    photon_post_os_get(recvReq[i], prev, recv[i], PHOTON_SEND_SIZE, PHOTON_TAG, 0);

    photon_post_os_get(recvReq[i+2], prev, recv[i+2], PHOTON_SEND_SIZE, PHOTON_TAG, 0);
    photon_send_FIN(recvReq[i+2], prev);

    photon_send_FIN(recvReq[i], prev);    
  }
  
  /* check that all the os_gets completed */
  for (i=0; i<NUM_REQ; i++) {
    while(1) {
      int flag, type;
      struct photon_status_t stat;
      int tst = photon_test(recvReq[i], &flag, &type, &stat);
      if( tst < 0 ) {
        fprintf(stderr,"%d: An error occured in photon_test(recv)\n", rank);
        exit(-1);
      }
      else if( tst > 0 ) {
        fprintf(stderr,"%d: That shouldn't have happened in this code\n", rank);
        exit(0);
      }
      else {
        if( flag ) {
          fprintf(stderr,"%d: get(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr, stat.tag, PHOTON_SEND_SIZE);
          break;
        }
        else {
          //fprintf(stderr,"%d: Busy waiting for recv\n", rank);
          //usleep(10*1000); // 1/100th of a second
        }
      }
    }
  }

  /* check that all the buffers we posted were retrieved */
  for (i=0; i<NUM_REQ; i++) {
    while(1) {
      int flag, type;
      struct photon_status_t stat;
      int tst = photon_test(sendReq[i], &flag, &type, &stat);
      if( tst < 0 ) {
        fprintf(stderr,"%d: An error occured in photon_test(recv)\n", rank);
        exit(-1);
      }
      else if( tst > 0 ) {
        fprintf(stderr,"%d: That shouldn't have happened in this code\n", rank);
        exit(0);
      }
      else {
        if( flag ) {
          fprintf(stderr,"%d: post_send_buf(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr, stat.tag, PHOTON_SEND_SIZE);
          break;
        }
        else {
          //fprintf(stderr,"%d: Busy waiting for recv\n", rank);
          //usleep(10*1000); // 1/100th of a second
        }
      }
    }
  }
  
  MPI_Barrier(MPI_COMM_WORLD);

  for (i=0; i<NUM_REQ; i++) {
    photon_unregister_buffer(send[i], PHOTON_SEND_SIZE);
    photon_unregister_buffer(recv[i], PHOTON_SEND_SIZE);
    free(send[i]);
    free(recv[i]);
  }

  photon_finalize();
  MPI_Finalize();
  return 0;
}

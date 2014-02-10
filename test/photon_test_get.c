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

int main(int argc, char *argv[]) {
  uint32_t recvReq,sendReq;
  char *send,*recv;
  int rank,size,prev,next;

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
    .use_cma = 0,
    .eth_dev = "roce0",
    .ib_dev = "qib0",
    .ib_port = 1,
    .backend = "ugni"
  };

  photon_init(&cfg);

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  // everyone posts their send buffer to their next rank
  photon_post_send_buffer_rdma(next, send, PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq);

  // do some "work"
  //sleep(1);

  // wait for the send buffer that was posted from the previous rank
  photon_wait_send_buffer_rdma(prev, PHOTON_TAG);

  // get that posted send buffer
  photon_post_os_get(prev, recv, PHOTON_SEND_SIZE, PHOTON_TAG, 0, &recvReq);
  photon_send_FIN(prev);

  while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(sendReq, &flag, &type, &stat);
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
        usleep(10*1000); // 1/100th of a second
      }
    }
  }
  while(1) {
    int flag, type;
    struct photon_status_t stat;
    int tst = photon_test(recvReq, &flag, &type, &stat);
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
        usleep(10*1000); // 1/100th of a second
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);

  photon_finalize();
  MPI_Finalize();
  return 0;
}

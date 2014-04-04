#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "photon.h"

#define PHOTON_SEND_SIZE 2048
#define PHOTON_TAG       13

int main(int argc, char *argv[]) {
  uint32_t recvReq,sendReq;
  char *send,*recv;
  int rank,size,prev,next;

  photon_addr mgid;

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
    .use_ud = 1,
    .eth_dev = "roce0",
    .ib_dev = "mlx4_1",
    .ib_port = 1,
    .backend = "verbs",
  };

  photon_init(&cfg);

  posix_memalign((void **) &send, 64, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 64, PHOTON_SEND_SIZE*sizeof(char));
  
  // everyone register to receive on the same address (224.0.2.2)
  inet_pton(AF_INET6, "ff0e::ffff:e000:0202", &mgid.raw);
  photon_register_addr(&mgid, AF_INET6);

  // send to the address
  photon_send(&mgid, send, PHOTON_SEND_SIZE, 0, &sendReq);

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
        fprintf(stderr,"%d: put(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
        break;
      }
      else {
        //fprintf(stderr,"%d: Busy waiting for recv\n", rank);
        usleep(10*1000); // 1/100th of a second
      }
    }
  }

  // once our send completes, let's see if someone sent us something
  struct photon_status_t stat;
  while(1) {
    photon_addr addr = {.s_addr = 0};
    int flag, type;
    int tst = photon_probe(&addr, &flag, &stat);
    //int tst = photon_test(recvReq, &flag, &type, &stat);
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
        fprintf(stderr,"%d: recv(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
        break;
      }
      else {
        //fprintf(stderr,"%d: Busy waiting for recv\n", rank);
        usleep(10*1000); // 1/100th of a second
      }
    }
  }

  // probe says we got some message, let's get it
  photon_recv(stat.request, recv, PHOTON_SEND_SIZE, 0);
  
  // do something with message...

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_addr(&mgid, AF_INET6);
  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);

  photon_finalize();
  MPI_Finalize();
  return 0;
}

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
// This testcase tests RDMA one-sided photon_post_os_get_direct functions
//****************************************************************************
START_TEST(test_rdma_one_sided_get_direct) 
{
  photon_rid recvReq,sendReq, request, req;
  char *send,*recv;
  int rank,size,prev,next;

  printf("Starting the photon one sided get direct test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
  }

  // everyone posts their recv buffer to their next rank
  int ret_proc;
  struct photon_buffer_t rbuf;
  photon_post_recv_buffer_rdma(next, send, PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq);
  
  // check for this below instead
  //photon_wait_any(&ret_proc, &req);
  
  // Wait for a recv buffer that was posted
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &recvReq);
  
  // Get the remote buffer info so that we can get
  photon_get_buffer_remote(recvReq, &rbuf);
  //photon_post_os_get(recvReq, prev, recv, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  //photon_send_FIN(recvReq, prev);
  photon_post_os_get_direct(prev, recv, PHOTON_SEND_SIZE, &rbuf, 0, &request);
 
  while (1) {
    int flag, type;
    struct photon_status_t stat;
    photon_test(request, &flag, &type, &stat);
    if (flag > 0) {
      printf("direct get of size %d completed successfully\n", (int)stat.size);
      photon_send_FIN(recvReq, prev); 
      break;
    }
    else {
      usleep(10);
    }
  }
  while (1) {
    int flag, type;
    struct photon_status_t stat;
    photon_test(sendReq, &flag, &type, &stat);
    if (flag > 0) {
      printf("post recv of size %d completed successfully\n", (int)stat.size);
      break;
    }
    else {
      usleep(10);
    }
  }
 
  MPI_Barrier(MPI_COMM_WORLD);

  printf("Received buffer: ");
  int j;
  for (j = 0; j < PHOTON_SEND_SIZE; j++) {
    printf("%d", recv[j]);
  }
  printf("\n");

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);

}
END_TEST

// Register the testcase
void add_photon_rdma_one_sided_get(TCase *tc) {
  tcase_add_test(tc, test_rdma_one_sided_get_direct);
}

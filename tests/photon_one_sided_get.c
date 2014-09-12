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

#define PHOTON_RECV_SIZE 32 
#define PHOTON_TAG       13
//****************************************************************************
// This testcase tests RDMA one-sided photon_post_os_get_direct functions
//****************************************************************************
START_TEST(test_rdma_one_sided_get_direct) 
{
  photon_rid sendReq, recvReq;
  char *send, *recv;
  int rank, size , next, prev;

  printf("Starting the photon one sided get direct test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  send = (char*)malloc(PHOTON_RECV_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_RECV_SIZE*sizeof(char));
  
  photon_register_buffer(send, PHOTON_RECV_SIZE);
  photon_register_buffer(recv, PHOTON_RECV_SIZE);

  printf("%d: sending data: ", rank);
  int i;
  for (i = 0; i < PHOTON_RECV_SIZE; i++) {
    send[i] = i;
    printf("%d", send[i]);
  }
  printf("\n");

  struct photon_buffer_t desc;
  desc.addr = (uintptr_t)0x00007f6605bff000;
  desc.size = PHOTON_RECV_SIZE;
  desc.priv.key0 = 2576896;
  desc.priv.key1 = 2576896;
  
  photon_post_send_buffer_rdma(next, send, PHOTON_RECV_SIZE, PHOTON_TAG, &sendReq);
  photon_wait_send_buffer_rdma(prev, PHOTON_TAG, &recvReq);
  photon_get_buffer_remote(recvReq, &desc);
  photon_post_os_get_direct(prev, recv, PHOTON_RECV_SIZE, &desc, 0, &recvReq);
  //photon_post_os_get(recvReq, prev, recv, PHOTON_RECV_SIZE, PHOTON_TAG, 0);
  photon_send_FIN(recvReq, prev);

  int flag;
  photon_rid req = 0;
  int send_comp = 0;
  while (send_comp) {
    int rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &req, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (req == PHOTON_TAG)
        send_comp--;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  printf("%d received buffer: ", rank);
  int j;
  for (j = 0; j < PHOTON_RECV_SIZE; j++) {
    printf("%d", recv[j]);
  }
  printf("\n");

  photon_unregister_buffer(send, PHOTON_RECV_SIZE);
  photon_unregister_buffer(recv, PHOTON_RECV_SIZE);
  free(send);
  free(recv);

}
END_TEST

// Register the testcase
void add_photon_rdma_one_sided_get(TCase *tc) {
  tcase_add_test(tc, test_rdma_one_sided_get_direct);
}

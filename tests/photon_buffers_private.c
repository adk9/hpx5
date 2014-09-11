#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"
#include "photon.h"
#define PHOTON_SEND_SIZE 32
#define PHOTON_TAG       13

//****************************************************************************
// This unit testcase tests photon buffers functions:
// 1. photon_register_buffer
// 2. photon_unregister_buffer
// 3. photon_get_buffer_private
//****************************************************************************
START_TEST (test_photon_get_private_buffers) 
{
  int rank, next, prev, size;
  char *send, *recv;
  photon_rid sendReq, recvReq;
  int flag, type;
  struct photon_buffer_t rbuf;

  printf("Starting the photon get private buffer test\n");
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) %size;

  posix_memalign((void **) &send, 32, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 32, PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
  }

  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &sendReq);

  photon_get_buffer_remote(sendReq, &rbuf);

  // It is a utility function which just exposes the private info of the 
  // registered buffer.
  int err = photon_get_buffer_private(&rbuf, PHOTON_SEND_SIZE, 0);
  ck_assert_msg(err == PHOTON_ERROR, "Could not find in buffer table"); 
  printf("Keys: 0x%016lx / 0x%016lx\n", rbuf.priv.key0, rbuf.priv.key1);

  // put directly into that recv buffer
  //photon_post_os_put(sendReq, prev, send, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  //photon_send_FIN(sendReq, prev);

  photon_put_with_completion(next, send, PHOTON_SEND_SIZE, (void*)rbuf.addr,
                               rbuf.priv, PHOTON_TAG, 0xcafebabe, 0);

  while (1) {
    struct photon_status_t stat;
    photon_test(sendReq, &flag, &type, &stat);
    if (flag) {
      printf("%d put of size %d completed successfully\n", rank, PHOTON_SEND_SIZE);
      break;
    } else {
      usleep(10*1000);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  printf("%d received buffer: ", rank);
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

//****************************************************************************
// Register the testcase photon_buffer.c
//****************************************************************************
void add_photon_buffers_private_test(TCase *tc) {
  tcase_add_test(tc, test_photon_get_private_buffers);
}

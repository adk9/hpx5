#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <unistd.h>
#include <check.h>
#include <mpi.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 32
#define PHOTON_TAG       13

int rank, other_rank, size;
struct photon_buffer_t lbuf;
struct photon_buffer_t rbuf;
char *send, *recv;
char *sendP, *recvP;
//****************************************************************************
// This unit testcase tests photon buffers functions:
// 1. photon_register_buffer
// 2. photon_unregister_buffer
// Utility method to get the remote buffer info set after a wait buffer 
// request.
// 3. photon_get_buffer_remote
//****************************************************************************
START_TEST (test_photon_get_remote_buffers) 
{
  photon_rid sendReq, recvReq, req;
  int flag, rc, remaining, src;
  int send_comp = 0;
  int recv_comp = 0;

  fprintf(detailed_log,"Starting the photon remote buffer test\n");
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  other_rank = (rank + 1) % size;

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  fprintf(detailed_log, "%d Sending buffer: ", rank);
  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
    fprintf(detailed_log,"%d", send[i]);
  }
  fprintf(detailed_log,"\n");

  // Post the recv buffer
  photon_post_recv_buffer_rdma(other_rank, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  // wait for a recv buffer that was posted
  photon_wait_recv_buffer_rdma(other_rank, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  // Get the remote buffer info so we can do our own put.
  photon_get_buffer_remote(sendReq, &rbuf);
  photon_send_FIN(sendReq, other_rank, PHOTON_REQ_COMPLETED);
  photon_wait(recvReq);

  lbuf.addr = (uintptr_t)send;
  lbuf.size = PHOTON_SEND_SIZE;
  lbuf.priv = (struct photon_buffer_priv_t){0,0};
  photon_put_with_completion(other_rank, PHOTON_SEND_SIZE, &lbuf,
			     &rbuf, PHOTON_TAG, 0xcafebabe, 0);
  send_comp++;
  recv_comp++;
  while (send_comp || recv_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &req, &src, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (req == PHOTON_TAG)
        send_comp--;
      else if (req == 0xcafebabe)
	recv_comp--;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  fprintf(detailed_log,"%d received buffer: ", rank);
  int j;
  for (j = 0; j < PHOTON_SEND_SIZE; j++) { 
    fprintf(detailed_log, "%d", recv[j]);
  }
  fprintf(detailed_log, "\n");

  ck_assert_msg(strcmp(send, recv) == 0, "Photon remote buffer test failed");

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);
}
END_TEST

//****************************************************************************
// Register the testcase photon_buffer.c
//****************************************************************************
void add_photon_buffers_remote_test(TCase *tc) {
  tcase_add_test(tc, test_photon_get_remote_buffers);
}

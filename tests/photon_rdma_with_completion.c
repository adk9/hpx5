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
#define PHOTON_TAG       UINT32_MAX

//****************************************************************************
// This testcase tests RDMA with completion function
//****************************************************************************
START_TEST(test_rdma_with_completion) 
{
  int rank, size, i, next, prev;
  int flag, rc, remaining;
  int send_comp = 0;
  int recv_comp = 0;
  fprintf(detailed_log, "Starting RDMA with completion test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  struct photon_buffer_t lbuf;
  struct photon_buffer_t rbuf;
  photon_rid sendReq, recvReq, req;
  char *send, *recv;

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  fprintf(detailed_log, "%d Sending buffer: ", rank);
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
    fprintf(detailed_log, "%d", send[i]);
  }
  fprintf(detailed_log, "\n");


  // Post the recv buffer
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  // wait for a recv buffer that was posted
  photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  // Get the remote buffer info so we can do our own put.
  photon_get_buffer_remote(sendReq, &rbuf);
  photon_send_FIN(sendReq, prev, PHOTON_REQ_COMPLETED);
  photon_wait(recvReq);

  // Put
  lbuf.addr = (uintptr_t)send;
  lbuf.size = PHOTON_SEND_SIZE;
  lbuf.priv = (struct photon_buffer_priv_t){0,0};
  photon_put_with_completion(prev, PHOTON_SEND_SIZE, &lbuf,
			     &rbuf, PHOTON_TAG, 0xcafebabe, 0);
  send_comp++;
  recv_comp++;
  while (send_comp || recv_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &req, PHOTON_PROBE_ANY);
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

  // Get
  send_comp = 0;
  lbuf.addr = (uintptr_t)send;
  lbuf.size = PHOTON_SEND_SIZE;
  lbuf.priv = (struct photon_buffer_priv_t){0,0};
  photon_get_with_completion(prev, PHOTON_SEND_SIZE, &lbuf,
                             &rbuf, PHOTON_TAG, 0xfacefeed, PHOTON_REQ_PWC_NO_RCE);
  send_comp++;
  while (send_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &req, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (req == PHOTON_TAG)
        send_comp--;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  fprintf(detailed_log, "%d received buffer: ", rank);
  int j;
  for (j = 0; j < PHOTON_SEND_SIZE; j++) {
    fprintf(detailed_log, "%d", recv[j]);
  }
  fprintf(detailed_log, "\n");

  ck_assert_msg(strcmp(send, recv) == 0, "Photon rdma with completion test failed");

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);

}
END_TEST

void add_photon_rdma_with_completion(TCase *tc) {
  tcase_add_test(tc, test_rdma_with_completion);
}

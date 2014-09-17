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

#include <arpa/inet.h>
#include <openssl/md5.h>

#include "photon.h"
#include "cutter_ids.h"


#define PHOTON_SEND_SIZE 1024

START_TEST (test_photon_communication)
{
  int myrank;
  char *send, *recv;
  photon_rid sendReq;
  struct photon_status_t status;
  bravo_node *node = NULL, *myNode = NULL; 
  photon_addr naddr;
  char buf[INET_ADDRSTRLEN];
  MD5_CTX ctx;
  unsigned char hash[16];

  fprintf(detailed_log, "Starting the Photon test\n");
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

  posix_memalign((void **) &send, 64, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 64, PHOTON_SEND_SIZE*sizeof(char));

  int i;
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
  }

  MD5_Init(&ctx);
  MD5_Update(&ctx, send, strlen(send));
  MD5_Final(hash, &ctx);

  photon_get_dev_addr(AF_INET, &naddr);
  inet_ntop(AF_INET, &naddr.s_addr, buf, sizeof(buf));
  
  init_bravo_ids(1, myrank, buf);
  myNode = find_bravo_node(&naddr);

  if (!myNode) {
    fprintf(detailed_log, "Could not find my node\n");
  }

 switch (myNode->index) {
  case 0:
    node = get_bravo_node(1);
    break;
  case 1:
    node = get_bravo_node(0);
    break;
  default:
    break;
  }

  if (!node) {
    fprintf(detailed_log, "could not find dest node\n");
  }

  for (i = 0; i < myNode->nblocks; i++) {
    photon_register_addr(&myNode->block[i], AF_INET6);
  }

  // need each node to be ready to receive before we send...
  MPI_Barrier(MPI_COMM_WORLD);

  // photon send uses addr, ptr, size, flags, request as inputs
  photon_send(&node->block[0], send, PHOTON_SEND_SIZE, 0, &sendReq);
  while (1) {
    int flag, type;
    photon_test(sendReq, &flag, &type, &status);
    if (flag > 0) {
      fprintf(detailed_log, "%d: put of size %d completed successfully\n", myrank, PHOTON_SEND_SIZE);
      break;
    } 
  }
  
  struct photon_status_t recv_status;
  photon_rid recvReq = 0;
  while (1) {
    //photon_addr addr = {.s_addr = 0};
    int flag, type;
    //photon_probe(&addr, &flag, &recv_status);
    photon_test(recvReq, &flag, &type, &recv_status);
    if (flag > 0) {
      // Probe says we got some message, let's get it
      photon_recv(recv_status.request, recv, recv_status.size, 0);
      fprintf(detailed_log, "%d: Received of size %lu\n", myrank, recv_status.size);
      break;
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
// Register the testcase photon_communication.c
//****************************************************************************
void add_photon_comm_test(TCase *tc) {
  tcase_add_test(tc, test_photon_communication);
}


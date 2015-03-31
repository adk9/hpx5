#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <check.h>
#include "tests.h"
#include "photon.h"
#include <assert.h>

#define PHOTON_BUF_SIZE (1024*64) // 64k
#define PHOTON_TAG UINT32_MAX
#define SQ_SIZE 30

static int ITERS = 10000;

static int sizes[] = {
  0,
  1,
  8,
  64,
  128,
  192,
  256,
  2048,
  4096,
  8192,
  12288,
  16384
};

int sendComp = 0;
int recvComp = 0;
int myrank;

void *wait_local() {
  photon_rid request;
  int flag, rc, remaining;

  while (sendComp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &request, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (request == PHOTON_TAG)
        sendComp--;
    }
  }
  return NULL;
}

int send_done(int n, int r) {
  int i;
  for (i=0; i<n; i++) {
    if (i==r)
      continue;
    photon_put_with_completion(i, 0, NULL, NULL, PHOTON_TAG, 0xdeadbeef, 0);
    sendComp++;
  }
  return 0;
}

int wait_done() {
  photon_rid request;
  int flag, remaining;
  do {
    photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &request, PHOTON_PROBE_ANY);
  } while (request != 0xdeadbeef);

  return 0;
}

int handle_ack_loop(int wait) {
  photon_rid request;
  int flag, rc, remaining;

  while (wait) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining, &request, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;
    if (flag) {
      if (request == 0xdeadbeef) {
        wait--;
      }
      else if (request == PHOTON_TAG)
        sendComp--;
      else {
        int ret = request>>32;
        photon_put_with_completion(ret, 0, NULL, NULL, PHOTON_TAG, request, 0);
        sendComp++;
      }
    }
  }

  wait_local(NULL);

  return 0;
}

//****************************************************************************
// This testcase tests RDMA with completion function
//****************************************************************************
START_TEST(test_photon_put_wc) 
{
  fprintf(detailed_log, "Starting the photon put wc test\n");
  int i, j, k, ns;
  int rank, nproc, ret_proc;
  int ASYNC_ITERS = SQ_SIZE;

  int rdata;
  int ur = open("/dev/urandom", O_RDONLY);
  read(ur, &rdata, sizeof rdata);
  srand(rdata);
  close(ur);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  myrank = rank;

  struct photon_buffer_t lbuf;
  struct photon_buffer_t rbuf[nproc];
  photon_rid recvReq[nproc], sendReq[nproc], request;
  char *send, *recv[nproc];

  // only need one send buffer
  posix_memalign((void **) &send, 64, PHOTON_BUF_SIZE*sizeof(uint8_t));
  photon_register_buffer(send, PHOTON_BUF_SIZE);

  // ... but recv buffers for each potential sender
  for (i=0; i<nproc; i++) {
    posix_memalign((void **) &recv[i], 64, PHOTON_BUF_SIZE*sizeof(uint8_t));
    photon_register_buffer(recv[i], PHOTON_BUF_SIZE);
  }

  for (i=0; i<nproc; i++) {
    // everyone posts their recv buffers
    photon_post_recv_buffer_rdma(i, recv[i], PHOTON_BUF_SIZE, PHOTON_TAG, &recvReq[i]);
    // make sure we clear the local post event
    photon_wait_any(&ret_proc, &request);
  }

  for (i=0; i<nproc; i++) {
    // wait for a recv buffer that was posted
    photon_wait_recv_buffer_rdma(i, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq[i]);
    // get the remote buffer info so we can do our own put
    photon_get_buffer_remote(sendReq[i], &rbuf[i]);
    photon_send_FIN(sendReq[i], i, PHOTON_REQ_COMPLETED);
    photon_wait(recvReq[i]);
  }

  // now we can proceed with our benchmark
  if (rank == 0)
    printf("%-7s%-9s%-7s%-11s%-12s%-12s\n", "Ranks", "Senders", "Bytes", "Sync (us)", "Sync GET", "Async (us)");

  struct timespec time_s, time_e;

  for (ns = 0; ns < nproc; ns++) {
    for (i=0; i<sizeof(sizes)/sizeof(sizes[0]); i++) {
      if (rank == 0) {
        printf("%-7d", nproc);
        printf("%-9u", ns + 1);
        printf("%-7u", sizes[i]);
        fflush(stdout);
      }

      // send to random rank, including self
      j = rand() % nproc;
      // send to random rank, excluding self
      while (j == rank)
        j = rand() % nproc;

      // PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<ITERS; k++) {
	  lbuf.addr = (uintptr_t)send;
	  lbuf.size = sizes[i];
	  lbuf.priv = (struct photon_buffer_priv_t){0,0};
          photon_put_with_completion(j, sizes[i], &lbuf, &rbuf[j], PHOTON_TAG, 0xcafebabe, PHOTON_REQ_NIL);
          sendComp++;
          wait_local(NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &time_e);
        wait_local(NULL);
      }

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f     ", latency);
        fflush(stdout);
      }

      assert(sendComp == 0 && recvComp == 0);
      
      // GET
      if (rank <= ns) {
        if (i && !(sizes[i] % 8)) {
          clock_gettime(CLOCK_MONOTONIC, &time_s);
          for (k=0; k<ITERS; k++) {
	    lbuf.addr = (uintptr_t)send;
	    lbuf.size = sizes[i];
	    lbuf.priv = (struct photon_buffer_priv_t){0,0};
            photon_get_with_completion(j, sizes[i], &lbuf, &rbuf[j], PHOTON_TAG, 0xfacefeed, 0);
            sendComp++;
            wait_local(NULL);
          }
          clock_gettime(CLOCK_MONOTONIC, &time_e);
        }
      }

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0 && i && !(sizes[i] % 8)) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f     ", latency);
        fflush(stdout);
      }
      else if (rank == 0) {
        printf("N/A       ");
      }

      assert(sendComp == 0 && recvComp == 0);

      // Async PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<ASYNC_ITERS; k++) {
	  lbuf.addr = (uintptr_t)send;
	  lbuf.size = sizes[i];
	  lbuf.priv = (struct photon_buffer_priv_t){0,0};
          if (photon_put_with_completion(j, sizes[i], &lbuf, &rbuf[j], PHOTON_TAG, 0xcafebabe, PHOTON_REQ_NIL)) {
            fprintf(stderr, "error: exceeded max outstanding work events (k=%d)\n", k);
            exit(1);
          }
          sendComp++;
        }
        clock_gettime(CLOCK_MONOTONIC, &time_e);
        wait_local(NULL);
      }
      
      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double overhead = time_us/ASYNC_ITERS;
        printf("%1.4f\n", overhead);
        fflush(stdout);
      }

      assert(sendComp == 0 && recvComp == 0);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(send, PHOTON_BUF_SIZE);
  free(send);
  for (i=0; i<nproc; i++) {
    photon_unregister_buffer(recv[i], PHOTON_BUF_SIZE);
    free(recv[i]);
  }
}
END_TEST

void add_photon_put_wc(TCase *tc) {
  tcase_add_test(tc, test_photon_put_wc);
}

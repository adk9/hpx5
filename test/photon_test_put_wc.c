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
#include <assert.h>

#include "test_cfg.h"

static void __attribute__((used)) dbg_wait(void) {
  int i = 0;
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  while (0 == i)
    sleep(12);
}

#define PHOTON_BUF_SIZE (1024*1024*4) // 4M
#define PHOTON_TAG       UINT32_MAX

static int LARGE_LIMIT = 8192;
static int LARGE_ITERS = 100;
static int ITERS       = 1000;

int iters;
int send_comp = 0;
int recv_comp = 0;
int myrank;

// could be a thread
void *wait_local(void *arg) {
  photon_rid request;
  int flag, rc, src;

  while (send_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, NULL, &request, &src, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (request == PHOTON_TAG)
        send_comp--;
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
    send_comp++;
  }
  return 0;
}

int wait_done() {
  photon_rid request;
  int flag, src;
  do {
    photon_probe_completion(PHOTON_ANY_SOURCE, &flag, NULL, &request, &src, PHOTON_PROBE_ANY);
  } while (request != 0xdeadbeef);
  
  return 0;
}

int main(int argc, char *argv[]) {
  int i, j, k, ns;
  int rank, nproc;

  int rdata;
  int ur = open("/dev/urandom", O_RDONLY);
  int rc = read(ur, &rdata, sizeof rdata);
  srand(rdata);
  close(ur);

  if (argc > 1) {
    ITERS = atoi(argv[1]);
    printf("ITERS: %d\n", ITERS);
  }

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  cfg.nproc = nproc;
  cfg.address = rank;

  myrank = rank;

  if (myrank == 1) {
    //dbg_wait();
  }

  if (photon_init(&cfg) != PHOTON_OK) {
    exit(1);
  }

  struct photon_buffer_t lbuf;
  struct photon_buffer_t rbuf[nproc];
  photon_rid recvReq[nproc], sendReq[nproc];
  char *send, *recv[nproc];

  // only need one send buffer
  rc = posix_memalign((void **) &send, 64, PHOTON_BUF_SIZE*sizeof(uint8_t));
  photon_register_buffer(send, PHOTON_BUF_SIZE);

  // ... but recv buffers for each potential sender
  for (i=0; i<nproc; i++) {
    rc = posix_memalign((void **) &recv[i], 64, PHOTON_BUF_SIZE*sizeof(uint8_t));
    photon_register_buffer(recv[i], PHOTON_BUF_SIZE);
  }
  
  for (i=0; i<nproc; i++) {
    // everyone posts their recv buffers
    photon_post_recv_buffer_rdma(i, recv[i], PHOTON_BUF_SIZE, PHOTON_TAG, &recvReq[i]);
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
    printf("%-7s%-9s%-12s%-11s%-12s%-12s%-12s\n", "Ranks", "Senders", "Bytes", "Sync (us)", " Sync GET");

  struct timespec time_s, time_e;
  
  for (ns = 0; ns < 1; ns++) {
    
    if (rank > ns) {
      wait_done();
    }

    for (i=1; i<=PHOTON_BUF_SIZE; i+=i) {
      if (rank == 0) {
        printf("%-7d", nproc);
        printf("%-9u", ns + 1);
        printf("%-12u", i);
        fflush(stdout);
      }

      iters = (i > LARGE_LIMIT) ? LARGE_ITERS : ITERS;
      
      // send to random rank, including self
      j = rand() % nproc;
      // send to random rank, excluding self
      while (j == rank)
        j = rand() % nproc;
      // send to your next rank only
      //j = next;

      // PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<iters; k++) {
	  lbuf.addr = (uintptr_t)send;
	  lbuf.size = i;
	  lbuf.priv = (struct photon_buffer_priv_t){0,0};
          photon_put_with_completion(j, i, &lbuf, &rbuf[j], PHOTON_TAG, 0xcafebabe, 0);
	  send_comp++;
          wait_local(NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &time_e);
      }

      //MPI_Barrier(MPI_COMM_WORLD);
      
      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/iters;
        printf("%1.2f\t", latency);
        fflush(stdout);
      }

      // GET
      if (rank <= ns) {
	// alignment chck
        //if (i && !(sizes[i] % 8)) {
	clock_gettime(CLOCK_MONOTONIC, &time_s);
	for (k=0; k<iters; k++) {
	  lbuf.addr = (uintptr_t)send;
	  lbuf.size = i;
	  lbuf.priv = (struct photon_buffer_priv_t){0,0};
	  photon_get_with_completion(j, i, &lbuf, &rbuf[j], PHOTON_TAG, 0xfacefeed, 0);
	  send_comp++;
	  wait_local(NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &time_e);
	//}
      }
      
      //MPI_Barrier(MPI_COMM_WORLD);
      
      //if (rank == 0 && i && !(sizes[i] % 8)) {
      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/iters;
        printf("%1.2f\n", latency);
        fflush(stdout);
      }
      else if (rank == 0) {
        printf("N/A       ");
        fflush(stdout);
      }
    }
    
    if (rank <= ns) {
      send_done(nproc, rank);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(send, PHOTON_BUF_SIZE);
  free(send);
  for (i=0; i<nproc; i++) {
    photon_unregister_buffer(recv[i], PHOTON_BUF_SIZE);
    free(recv[i]);
  }

  photon_finalize();
  MPI_Finalize();
  return 0;
}

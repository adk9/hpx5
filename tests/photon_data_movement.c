#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 16777216 // 16MB
#define PHOTON_TAG       13

#define PHOTON_LEDGER_SIZE 60     // we can make this larger if needed
#define PHOTON_BUF_SIZE (1024*64) // 64k
#define PHOTON_WC_TAG UINT32_MAX

static int ITERS = 1000;

static int sizes[] = {
  0,
  1,
  8,
  64,
  128,
  196,
  256,
  2048,
  4096,
  8192,
  12288,
  16384
};

int send_comp = 0;
int recv_comp = 0;
int myrank, size, rank;

//****************************************************************************
// photon get test
//****************************************************************************
START_TEST (test_photon_test_get) 
{
  photon_rid recvReq, sendReq;
  char *send, *recv;
  int rank, size, prev, next;
  printf("Starting the photon get test\n");


  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;
  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  // everyone posts their send buffer to their next rank
  photon_post_send_buffer_rdma(next, send, PHOTON_SEND_SIZE, PHOTON_TAG, &sendReq);

  // do some "work"
  //sleep(1);

  // wait for the send buffer that was posted from the previous rank
  photon_wait_send_buffer_rdma(prev, PHOTON_TAG, &recvReq);

  // get that posted send buffer
  photon_post_os_get(recvReq, prev, recv, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  photon_send_FIN(recvReq, prev);

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
        fprintf(stderr,"%d: post_send_buf(%d, %d) of size %lu completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, stat.size);
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
        fprintf(stderr,"%d: get(%d, %d) of size %lu completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, stat.size);
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
}
END_TEST

//****************************************************************************
// photon put test
//****************************************************************************
START_TEST (test_photon_test_put) 
{
  printf("Starting the photon put test\n");
  photon_rid recvReq, sendReq;
  char *send, *recv;
  int rank, size, prev, next;

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  posix_memalign((void **) &send, 64, PHOTON_SEND_SIZE*sizeof(char));
  posix_memalign((void **) &recv, 64, PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);

  // do some "work"
  //sleep(1);

  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &sendReq);

  // put directly into that recv buffer
  photon_post_os_put(sendReq, prev, send, PHOTON_SEND_SIZE, PHOTON_TAG, 0);
  photon_send_FIN(sendReq, prev);

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
        fprintf(stderr,"%d: recv(%d, %d) of size %d completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag, PHOTON_SEND_SIZE);
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
}
END_TEST

//****************************************************************************
// photon put wc test
//****************************************************************************
// could be a thread
void *wait_local(void *arg __attribute__((unused))) {
  photon_rid request;
  int flag, rc;

  while (send_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (request == PHOTON_WC_TAG)
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
    photon_put_with_completion(i, NULL, 0, NULL, (struct photon_buffer_priv_t) {0,0}, PHOTON_WC_TAG, 0xdeadbeef, 0);
    send_comp++;
  }
  return 0;
}

int wait_done() {
  photon_rid request;
  int flag, rc __attribute__((unused));
  do {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_ANY);
  } while (request != 0xdeadbeef);

  return 0;
}

int handle_ack_loop(int wait) {
  photon_rid request;
  int flag, rc;

  while (wait) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &request, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;
    if (flag) {
      if (request == 0xdeadbeef) {
        wait--;
      }
      else if (request == PHOTON_WC_TAG)
        send_comp--;
      else {
        int ret = request>>32;
        photon_put_with_completion(ret, NULL, 0, NULL, (struct photon_buffer_priv_t) {0,0}, PHOTON_WC_TAG, request, 0);
        send_comp++;
      }
    }
  }

  wait_local(NULL);

  return 0;
}


START_TEST (test_photon_test_put_wc)
{
  printf("Starting the photon put wc test\n");
  int i, j, k, ns;
  int rank, nproc, ret_proc;
  int ASYNC_ITERS = ITERS;
  int rdata;
  int ur = open("/dev/urandom", O_RDONLY);
  read(ur, &rdata, sizeof rdata);
  srand(rdata);
  close(ur);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  myrank = rank;

  struct photon_buffer_t rbuf[nproc];
  photon_rid recvReq[nproc], sendReq[nproc], request;
  char *send, *recv[nproc];

  // only need one send buffer
  posix_memalign((void **) &send, 8, PHOTON_BUF_SIZE*sizeof(uint8_t));
  photon_register_buffer(send, PHOTON_BUF_SIZE);

  // ... but recv buffers for each potential sender
  for (i=0; i<nproc; i++) {
    posix_memalign((void **) &recv[i], 8, PHOTON_BUF_SIZE*sizeof(uint8_t));
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
    photon_wait_recv_buffer_rdma(i, PHOTON_TAG, &sendReq[i]);
    // get the remote buffer info so we can do our own put
    photon_get_buffer_remote(sendReq[i], &rbuf[i]);
  }

  // now we can proceed with our benchmark
  if (rank == 0)
    printf("%-7s%-9s%-7s%-11s%-12s%-12s%-12s\n", "Ranks", "Senders", "Bytes", "Sync (us)", "Sync GET", "Async (us)", "RTT (us)");

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
      // send to your next rank only
      //j = next;

      // PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<ITERS; k++) {
          photon_put_with_completion(j, send, sizes[i], (void*)rbuf[j].addr, rbuf[j].priv, PHOTON_TAG, 0xcafebabe, 0);
          send_comp++;
          wait_local(NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &time_e);
        send_done(nproc, rank);
        wait_local(NULL);
      }
      else
        wait_done();

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f     ", latency);
        fflush(stdout);
      }

      ck_assert(send_comp == 0 && recv_comp == 0);

      // GET
      if (rank <= ns) {
        if (i) {
          clock_gettime(CLOCK_MONOTONIC, &time_s);
          for (k=0; k<ITERS; k++) {
            photon_get_with_completion(j, send, sizes[i], (void*)rbuf[j].addr, rbuf[j].priv, PHOTON_TAG, 0);
            send_comp++;
            wait_local(NULL);
          }
          clock_gettime(CLOCK_MONOTONIC, &time_e);
        }
      }

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0 && i) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double latency = time_us/ITERS;
        printf("%1.4f     ", latency);
        fflush(stdout);
      }
      else if (rank == 0) {
        printf("N/A       ");
      }

      ck_assert(send_comp == 0 && recv_comp == 0);

      // Async PUT
      if (rank <= ns) {
        clock_gettime(CLOCK_MONOTONIC, &time_s);
        for (k=0; k<ASYNC_ITERS; k++) {
          if (photon_put_with_completion(j, send, sizes[i], (void*)rbuf[j].addr, rbuf[j].priv, PHOTON_TAG, 0xcafebabe, 0)) {
            fprintf(stderr, "error: exceeded max outstanding work events (k=%d)\n", k);
            exit(1);
          }
          send_comp++;
        }
        clock_gettime(CLOCK_MONOTONIC, &time_e);
        send_done(nproc, rank);
        wait_local(NULL);
      }
      else
        wait_done();

      MPI_Barrier(MPI_COMM_WORLD);

      if (rank == 0) {
        double time_ns = (double)(((time_e.tv_sec - time_s.tv_sec) * 1e9) + (time_e.tv_nsec - time_s.tv_nsec));
        double time_us = time_ns/1e3;
        double overhead = time_us/ASYNC_ITERS;
        printf("%1.4f\n", overhead);
        fflush(stdout);
      }

      ck_assert(send_comp == 0 && recv_comp == 0);
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

//****************************************************************************
// Register the testcase photon_data_movement.c
//****************************************************************************
void add_photon_data_movement(TCase *tc) {
  tcase_add_test(tc, test_photon_test_get);
  tcase_add_test(tc, test_photon_test_put);
  //tcase_add_test(tc, test_photon_test_put_wc);
}

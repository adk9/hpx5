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
#include <assert.h>
#include <time.h>

#define BENCHMARK "Photon OS PUT With Completion Bandwidth Test (1GB)"
#define MESSAGE_ALIGNMENT 64
#define MAX_MSG_SIZE (1<<22)
#define MYBUFSIZE 1024*1024*1024   /* ~= 1GB */
#define BUFSIZE   1024*1024*100    /* 100MB */
#define PHOTON_TAG 13

#define STARTSIZE 512

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 20
#define FLOAT_PRECISION 2

#define SQ_SIZE 16

//****************************************************************************
// photon put with completion test
//****************************************************************************
START_TEST (test_photon_test_put_wc_bw_bench) 
{
  photon_rid recvReq, sendReq, request, req;
  char *s_buf_heap, *r_buf_heap;
  char *s_buf, *r_buf;
  int align_size;
  double t = 0.0, t_start = 0.0, t_end = 0.0;
  int i, k, send_count;
  uint64_t num_bytes_sent;
  int rank, size, prev, next;
  int ret;
  struct photon_buffer_t lbuf;
  struct photon_buffer_t rbuf;
  fprintf(detailed_log, "Starting the photon put with completion bandwidth benchmark test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  align_size = MESSAGE_ALIGNMENT;

  /**************Allocating Memory*********************/
  posix_memalign((void**) &s_buf_heap, 64, BUFSIZE*sizeof(char));
  posix_memalign((void**) &r_buf_heap, 64, BUFSIZE*sizeof(char));

  photon_register_buffer(s_buf_heap, BUFSIZE);
  photon_register_buffer(r_buf_heap, BUFSIZE);

  s_buf = (char *) (((unsigned long) s_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  r_buf = (char *) (((unsigned long) r_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  /**************Memory Allocation Done*********************/

  assert((s_buf != NULL) && (r_buf != NULL));

  if (rank == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s%*s\n", 10, "# Msg Size", FIELD_WIDTH, "Time (s)",
            FIELD_WIDTH, "Bandwidth (MB/s)");
    fflush(stdout);
  }

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, r_buf, BUFSIZE, PHOTON_TAG, &recvReq);
  // Make sure we clear the local post event
  photon_wait_any(&ret, &request);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
  photon_get_buffer_remote(sendReq, &rbuf);

  for (k = STARTSIZE; k <= MAX_MSG_SIZE; k = (k ? k * 2 : 1)) {
    // touch the data 
    for (i = 0; i < k; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }

    MPI_Barrier(MPI_COMM_WORLD);

    num_bytes_sent = 0;
    send_count = 0;
    t_start = TIME();
    while(num_bytes_sent < MYBUFSIZE) {
      if (send_count < SQ_SIZE) {
        // put directly into that recv buffer
	lbuf.addr = (uintptr_t)s_buf;
	lbuf.size = k;
	lbuf.priv = (struct photon_buffer_priv_t){0,0};
        photon_put_with_completion(prev, k, &lbuf, &rbuf, PHOTON_TAG, 0xcafebabe, 0);
        send_count++;
        num_bytes_sent+=k;
      }
      else {
        while (1) {
          int flag, remaining;
          int rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining,
                                           &req, PHOTON_PROBE_EVQ);   
          if (rc != PHOTON_OK)
            continue;
          if (flag) {
            if (req == PHOTON_TAG) {
              send_count--;
              break;
            }
          }
        }
      }
    }
    // clear all remaining put requests
    do {
      int flag, remaining;
      int rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &remaining,
                                       &req, PHOTON_PROBE_EVQ);
      if (rc != PHOTON_OK)
        continue;
      if (flag) {
        if (req == PHOTON_TAG) {
          send_count--;
        }
      }
    } while (send_count);

    // Wait for the buffer to become empty
    t_end = TIME();
    t = t_end - t_start;

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = 1.0 * t/1e6;
      fprintf(stdout, "%-*d%*.*f%*.*f\n", 10, k, FIELD_WIDTH,
                    FLOAT_PRECISION, latency,
                    FIELD_WIDTH, FLOAT_PRECISION, MYBUFSIZE/1e6 / latency);
      fflush(stdout);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);

  photon_send_FIN(sendReq, prev, PHOTON_REQ_COMPLETED);
  photon_wait(recvReq);

  photon_unregister_buffer(s_buf_heap, BUFSIZE);
  photon_unregister_buffer(r_buf_heap, BUFSIZE);
  free(s_buf_heap);
  free(r_buf_heap);
}
END_TEST

void add_photon_put_wc_bw_bench(TCase *tc) {
  tcase_add_test(tc, test_photon_test_put_wc_bw_bench);
}

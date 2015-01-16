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
#include <time.h>

#define BENCHMARK "Photon recv buffer latency benchmark"
#define MESSAGE_ALIGNMENT 64
#define MAX_MSG_SIZE (1<<20)
#define MYBUFSIZE (MAX_MSG_SIZE + MESSAGE_ALIGNMENT)
#define PHOTON_TAG 13

int myrank, size, rank;

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 20
#define FLOAT_PRECISION 2

//****************************************************************************
// photon get test
//****************************************************************************
START_TEST (test_photon_recv_buffer_bench) 
{
  int skip = 1000;
  int loop = 10000;

  photon_rid recvReq, sendReq, req;
  char *r_buf_heap;
  char *r_buf;
  int align_size;
  int64_t t_start = 0, t_end = 0;
  int i, k;
  int ret_proc;

  int rank, size, prev, next;
  fprintf(detailed_log, "Starting the photon recv buffer benchmark test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size + rank - 1) % size;

  align_size = MESSAGE_ALIGNMENT;

  /**************Allocating Memory*********************/
  posix_memalign((void**) &r_buf_heap, 64, MYBUFSIZE*sizeof(char));
  photon_register_buffer(r_buf_heap, MYBUFSIZE);
  r_buf = (char *) (((unsigned long) r_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  /**************Memory Allocation Done*********************/
  if (rank == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
    fflush(stdout);
  }

  for (k = 1; k <= MAX_MSG_SIZE; k = (k ? k * 2 : 1)) {
    // touch the data 
    for (i = 0; i < k; i++) {
      r_buf[i] = 'b';
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (i = 0; i < loop + skip; i++) {
      if (i == skip) t_start = TIME();
      if (rank == 0) {
        // Source post the buffer
        photon_post_recv_buffer_rdma(next, r_buf, k, PHOTON_TAG, &recvReq);
        // Source wait_any to reap the event associated with the local post
        // buffer operation
        photon_wait_any(&ret_proc, &req);
        photon_wait(recvReq);
      }
      else {
        // Dest: Wait buffer
        // wait for the recv buffer that was posted from the previous rank
        photon_wait_recv_buffer_rdma(prev, PHOTON_ANY_SIZE, PHOTON_TAG, &sendReq);
        photon_send_FIN(sendReq, prev, PHOTON_REQ_COMPLETED);
        photon_wait_any(&ret_proc, &req);
      }
    } // End of for loop
    t_end = TIME();

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = (1.0 * (t_end-t_start)) / loop;
      fprintf(stdout, "%-*d%*.*f\n", 10, k, FIELD_WIDTH,
                    FLOAT_PRECISION, latency);
      fflush(stdout);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(r_buf_heap, MYBUFSIZE);
  free(r_buf_heap);
}
END_TEST

void add_photon_recv_buffer_bench(TCase *tc) {
  tcase_add_test(tc, test_photon_recv_buffer_bench);
}

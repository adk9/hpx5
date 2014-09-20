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

#define BENCHMARK "Photon OS PUT Bandwidth Test"
#define MESSAGE_ALIGNMENT 64
#define MAX_MSG_SIZE (1<<22)
#define MYBUFSIZE 1024*1024*1024   /* ~= 1GB */
#define PHOTON_TAG 13

#define STARTSIZE 1024
#define BUFFULLSIZE (4294967296)   /* ~= 4GB */

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 20
#define FLOAT_PRECISION 2

//****************************************************************************
// photon put test
//****************************************************************************
START_TEST (test_photon_test_put_bw_bench) 
{
  photon_rid recvReq, sendReq;
  char *s_buf_heap, *r_buf_heap;
  char *s_buf, *r_buf;
  int align_size;
  double t = 0.0, t_start = 0.0, t_end = 0.0;
  int i, k;
  uint64_t num_bytes_sent;
  int rank, size, prev, next;
  fprintf(detailed_log, "Starting the photon put bandwidth benchmark test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  align_size = MESSAGE_ALIGNMENT;

  /**************Allocating Memory*********************/
  posix_memalign((void**) &s_buf_heap, 64, MYBUFSIZE*sizeof(char));
  posix_memalign((void**) &r_buf_heap, 64, MYBUFSIZE*sizeof(char));

  photon_register_buffer(s_buf_heap, MYBUFSIZE);
  photon_register_buffer(r_buf_heap, MYBUFSIZE);

  s_buf = (char *) (((unsigned long) s_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  r_buf = (char *) (((unsigned long) r_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  /**************Memory Allocation Done*********************/

  assert((s_buf != NULL) && (r_buf != NULL));

  if (rank == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)",
            FIELD_WIDTH, "Bandwidth (MB/s)");
    fflush(stdout);
  }

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, r_buf, MYBUFSIZE, PHOTON_TAG, &recvReq);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &sendReq);

  for (k = STARTSIZE; k <= MAX_MSG_SIZE; k = (k ? k * 2 : 1)) {
    // touch the data 
    for (i = 0; i < k; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }

    MPI_Barrier(MPI_COMM_WORLD);

    num_bytes_sent = 0;
    t_start = TIME();
    while(num_bytes_sent < BUFFULLSIZE) {
      // put directly into that recv buffer
      photon_post_os_put(sendReq, prev, s_buf, k, PHOTON_TAG, 0);

      while (1) {
        int flag, type;
        struct photon_status_t stat;
        int tst = photon_test(sendReq, &flag, &type, &stat);
        if (tst < 0) {
          fprintf(detailed_log,"%d: An error occured in photon_test(recv)\n", rank);
          exit(-1);
        }
        else if ( tst > 0 ) {
          fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
          exit(0);
        }
        else {
          if (flag > 0) {
            num_bytes_sent+=k;
            break;
          }
        }
      }
    } 
    // Wait for the buffer to become empty
    t_end = TIME();
    t = t_end - t_start;

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = 1.0 * (t_end-t_start);
      fprintf(stdout, "%-*d%*.*f%*.*f\n", 10, k, FIELD_WIDTH,
                    FLOAT_PRECISION, latency,
                    FIELD_WIDTH, FLOAT_PRECISION, BUFFULLSIZE / t);
      fflush(stdout);
    }
  }
  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(s_buf_heap, MYBUFSIZE);
  photon_unregister_buffer(r_buf_heap, MYBUFSIZE);
  free(s_buf_heap);
  free(r_buf_heap);
}
END_TEST

void add_photon_os_put_bw_bench(TCase *tc) {
  tcase_add_test(tc, test_photon_test_put_bw_bench);
}

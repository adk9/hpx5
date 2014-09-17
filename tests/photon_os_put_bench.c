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

#define BENCHMARK "Photon OS PUT Test"
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
START_TEST (test_photon_test_put_bench) 
{
  int skip = 1000;
  int loop = 10000;
  int skip_large = 10;
  int loop_large = 100;
  int large_message_size = 8192;

  photon_rid recvReq, sendReq;
  char *s_buf_heap, *r_buf_heap;
  char *s_buf, *r_buf;
  int align_size;
  int64_t t_start = 0, t_end = 0;
  int i, k;

  int rank, size, prev, next;
  fprintf(detailed_log, "Starting the photon put benchmark test\n");

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
  if (rank == 0) {
    fprintf(stdout, HEADER);
    fprintf(stdout, "%-*s%*s\n", 10, "# Size", FIELD_WIDTH, "Latency (us)");
    fflush(stdout);
  }

  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, r_buf, MYBUFSIZE, PHOTON_TAG, &recvReq);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &sendReq);

  for (k = 1; k <= MAX_MSG_SIZE; k = (k ? k * 2 : 1)) {
    // touch the data 
    for (i = 0; i < k; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }

    if (k > large_message_size) {
      loop = loop_large = 100;
      skip = skip_large = 0;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      for (i = 0; i < loop + skip; i++) {
         if (i == skip) t_start = TIME();
         // put directly into that recv buffer
         photon_post_os_put(sendReq, prev, s_buf, k, PHOTON_TAG, 0);
         while (1) {
           int flag, type;
           struct photon_status_t stat;
           photon_test(sendReq, &flag, &type, &stat);
           if (flag > 0) {
             //photon_send_FIN(sendReq, prev);
             break;
           }
         }
      } // End of for loop
      t_end = TIME();
    } // End of if(rank == 0)

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      double latency = (1.0 * (t_end-t_start)) / loop;
      fprintf(stdout, "%-*d%*.*f\n", 10, k, FIELD_WIDTH,
                    FLOAT_PRECISION, latency);
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

void add_photon_os_put_bench(TCase *tc) {
  tcase_add_test(tc, test_photon_test_put_bench);
}

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

#define PHOTON_LEDGER_SIZE 64 // Default size

#define MAX_ALIGNMENT 128 
#define MAX_SIZE (1<<22)
#define MYBUFSIZE (4294967296)   /* ~= 4GB */
#define PHOTON_TAG UINT32_MAX

#define LOOP (30)

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 20
#define FLOAT_PRECISION 2

double difftv(struct timeval *start, struct timeval *end)
{
  double retval;
  retval = end->tv_sec - start->tv_sec;

  if (end->tv_usec >= start->tv_usec) {
    retval += ((double)(end->tv_usec - start->tv_usec)) / 1e6;
  } else {
    retval -= 1.0;
    retval += ((double)(end->tv_usec + 1e6) - start->tv_usec) / 1e6;
  }
  return retval;
}

char* print_bytes(uint64_t b, int bits) {
  char ret[64];
  char val = 'B';
  int bb = 1;

  if (bits) {
    bb = 8;
    val = 'b';
  }

  if (b > 1e9)
    sprintf(ret, "%.2f G%c", (double)b/1e9*bb, val);
  else if (b > 1e6)
    sprintf(ret, "%.2f M%c", (double)b/1e6*bb, val);
  else if (b > 1e3*100)
    sprintf(ret, "%.2f K%c", (double)b/1e3*100*bb, val);
  else
    sprintf(ret, "%d %cytes", (int)b, val);
  
  return strdup(ret);
}

void print_bw(struct timeval *s, struct timeval *e, uint64_t b) {
  uint64_t rate = (uint64_t)b/difftv(s, e);
  printf("[0.0-%.1f sec]\t%14s\t%14s/s\tbytes: %"PRIu64"\n", difftv(s, e),
               print_bytes(b, 0), print_bytes(rate, 1), b);
}

//****************************************************************************
// photon os put bandwidth test
//****************************************************************************
START_TEST (test_photon_os_put_bw_bench) 
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
  int i, j, k;


  struct timeval start_time, end_time;

  int rank, size, prev, next;
  fprintf(detailed_log, "Starting the photon os put bandwidth benchmark test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank + 1) % size;
  prev = (size + rank - 1) % size;

  if (size != 2) {
    if (rank == 0) {
      fprintf(stderr, "This test requires exactly two processes\n");
    }
  }

  loop = LOOP;
  align_size = MAX_ALIGNMENT;
  /**************Allocating Memory*********************/
  posix_memalign((void**) &s_buf_heap, 128, MYBUFSIZE*sizeof(char));
  posix_memalign((void**) &r_buf_heap, 128, MYBUFSIZE*sizeof(char));

  photon_register_buffer(s_buf_heap, MYBUFSIZE);
  photon_register_buffer(r_buf_heap, MYBUFSIZE);

  s_buf = (char *) (((unsigned long) s_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  r_buf = (char *) (((unsigned long) r_buf_heap + (align_size - 1)) /
                      align_size * align_size);
  /**************Memory Allocation Done*********************/

  assert((s_buf != NULL) && (r_buf != NULL));

  if (rank == 0) {
    fprintf(stdout, "# %s \n", BENCHMARK );
    fflush(stdout);
  }
  // everyone posts their recv buffer to their next rank
  photon_post_recv_buffer_rdma(next, r_buf, MYBUFSIZE, PHOTON_TAG, &recvReq);
  // wait for the recv buffer that was posted from the previous rank
  photon_wait_recv_buffer_rdma(prev, PHOTON_TAG, &sendReq);

  for (k = 1; k <= MAX_SIZE; k *= 2) {
    // touch the data 
    for (i = 0; i < k; i++) {
      s_buf[i] = 'a';
      r_buf[i] = 'b';
    }

    if (k > large_message_size) {
      loop = loop_large;
      skip = skip_large;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    for (i = 0; i < loop + skip; i++) {
      if (i == skip) gettimeofday(&start_time, NULL);
      // put directly into that recv buffer
      for (j = 0; j < PHOTON_LEDGER_SIZE; j++) {
        photon_post_os_put(sendReq, prev, s_buf, k, PHOTON_TAG, 0);
      }
      while (1) {
        int flag, type;
        struct photon_status_t stat;
        photon_test(sendReq, &flag, &type, &stat);
        if (flag > 0) {
          break;
        }
      }
    } // End of for loop
    gettimeofday(&end_time, NULL);

    MPI_Barrier(MPI_COMM_WORLD);

    if(rank == 0) 
      print_bw(&start_time, &end_time, k);
  }

  MPI_Barrier(MPI_COMM_WORLD);

  photon_unregister_buffer(s_buf_heap, MYBUFSIZE);
  photon_unregister_buffer(r_buf_heap, MYBUFSIZE);
  free(s_buf_heap);
  free(r_buf_heap);
}
END_TEST

void add_photon_os_put_bw_bench(TCase *tc) {
  tcase_add_test(tc, test_photon_os_put_bw_bench);
}

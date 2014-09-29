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
#include "photon.h"

void do_kernel(int);
void photon_gettime_(double *);
int rank, size;

double junk;

void kernel(int maxIter) {
  junk = 42;
  do_kernel(maxIter);
}

int estimateKernelSize(double time) {
  double dt;
  struct timeval tv1, tv2;

  gettimeofday(&tv1, NULL);
  kernel(10000);
  gettimeofday(&tv2, NULL);
  dt = (1000000.0*(double)(tv2.tv_sec-tv1.tv_sec) + (double)(tv2.tv_usec-tv1.tv_usec));

  return (int)((10000*time)/dt);
}

START_TEST (test_photon)
{
  photon_rid recvReq, sendReq;
  char *send, *recv;
  int prev, next;
  int arraySize, workSize, maxSize, maxWork, smallAmountOfWork, trial;
  double kernel_start, kernel_end, total_start, total_end, overhead;

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);

  next = (rank+1) % size;
  prev = (size+rank-1) % size;

  fprintf(detailed_log, "Starting the Photon test\n");

  maxSize = 104857600; // 100 Mb
  smallAmountOfWork = estimateKernelSize(5000); // 5 milliseconds (i think)
  //maxWork = estimateKernelSize(200000); // 20 seconds (i think)
  maxWork = 1024*1024;
  for (trial = 0; trial < 1; trial++) {
    for (arraySize = 1; arraySize <= maxSize; arraySize = arraySize*16) {
      send = (char*)malloc(arraySize*sizeof(char));
      recv = (char*)malloc(arraySize*sizeof(char));
      photon_register_buffer(send,arraySize);
      photon_register_buffer(recv,arraySize);
      //for (workSize = maxWork; workSize == maxWork; workSize = workSize/16) {
      for (workSize = maxWork; workSize > 1; workSize = workSize/16) {
        photon_gettime_(&total_start);
        photon_post_recv_buffer_rdma(prev,recv,arraySize,13,&recvReq);
        //photon_post_send_buffer_rdma(prev,recv,arraySize,13,&sendReq);
        kernel(smallAmountOfWork);
        photon_wait_recv_buffer_rdma(next,PHOTON_ANY_SIZE,13,&sendReq);
        photon_post_os_put(sendReq,next,send,arraySize,13,0);
        //photon_wait_send_buffer_rdma(next,PHOTON_ANY_SIZE,13,&recvReq);
        //photon_post_os_get(recvReq,next,send,arraySize,13,0);
        photon_gettime_(&kernel_start);
        kernel(workSize);
        photon_gettime_(&kernel_end);
        while(1) {
          int flag, type;
          struct photon_status_t stat;
          int tst = photon_test(sendReq, &flag, &type, &stat);
          if( tst < 0 ) {
            fprintf(detailed_log,"%d: An error occured in photon_test(send)\n", rank);
            exit(-1);
          }
          else if( tst > 0 ) {
            fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
            exit(0);
          }
         else {
            if (flag > 0) {
              fprintf(detailed_log,"%d: send(%d, %d) completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag);
              photon_send_FIN(sendReq,next,0);
              break;
            }
          }
        }
        while(1) {
          int flag, type;
          struct photon_status_t stat;
          int tst = photon_test(recvReq, &flag, &type, &stat);
          if( tst < 0 ) {
            fprintf(detailed_log,"%d: An error occured in photon_test(recv)\n", rank);
            exit(-1);
          }
          else if( tst > 0 ) {
            fprintf(detailed_log,"%d: That shouldn't have happened in this code\n", rank);
            exit(0);
          }
          else {
            if (flag > 0) {
              fprintf(detailed_log,"%d: recv(%d, %d) completed successfully\n", rank, (int)stat.src_addr.global.proc_id, stat.tag);
              break;
            }
          }
        }
        photon_gettime_(&total_end);
        overhead = (total_end-total_start) - (kernel_end-kernel_start);
        if (rank == 0 )
          fprintf(detailed_log, "%i,%i,%i,%f,%f,%f\n",trial,arraySize,workSize,(total_end-total_start),(kernel_end-kernel_start),overhead);
      }
      photon_unregister_buffer(send,arraySize);
      photon_unregister_buffer(recv,arraySize);
      free(send);
      free(recv);
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
}
END_TEST

//****************************************************************************
// Register the testcase photon_test.c
//****************************************************************************
void add_photon_test(TCase *tc) {
  tcase_add_test(tc, test_photon);
}


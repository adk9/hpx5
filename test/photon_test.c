#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "photon.h"

// @@ need a kernel that takes an integer input that varies the amount of
//    work that the kernel performs (ie. a loop index)

// @@ based upon this kernel function, we need to set the smallAmountOfWork
//    and maxWork values correctly

void do_kernel(int);
void photon_gettime_(double *);

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

int main(int argc, char *argv[]) {
	uint32_t recvReq,sendReq;
	char *send,*recv;
	int rank,size,prev,next;
	int arraySize,workSize,maxSize,maxWork,smallAmountOfWork,trial;
	double kernel_start, kernel_end, total_start, total_end, overhead;
	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&rank);
	MPI_Comm_size(MPI_COMM_WORLD,&size);
	next = (rank+1) % size;
	prev = (size+rank-1) % size;

	struct photon_config_t cfg = {
		.use_mpi = 1,
		.nproc = size,
		.address = rank,
		.comm = MPI_COMM_WORLD,
		.use_forwarder = 0,
		.ib_dev = "mlx4_1",
		.ib_port = 1,
		.backend = "verbs"
	};

	photon_init(&cfg);
	maxSize = 104857600; // 100 Mb
	smallAmountOfWork = estimateKernelSize(5000); // 5 milliseconds (i think)
	maxWork = estimateKernelSize(200000); // 20 seconds (i think)
	for (trial = 0; trial < 1; trial++) {
		for (arraySize = 1; arraySize <= maxSize; arraySize = arraySize*16) {
			send = (char*)malloc(arraySize*sizeof(char));
			recv = (char*)malloc(arraySize*sizeof(char));
			photon_register_buffer(send,arraySize);
			photon_register_buffer(recv,arraySize);
			for (workSize = maxWork; workSize > 1; workSize = workSize/16) {
				photon_gettime_(&total_start);
				photon_post_recv_buffer_rdma(prev,recv,arraySize,13,&recvReq);
				kernel(smallAmountOfWork);
				photon_wait_recv_buffer_rdma(next,13);
				photon_post_os_put(next,send,arraySize,13,0,&sendReq);
				photon_send_FIN(next);
				photon_gettime_(&kernel_start);
				kernel(workSize);
				photon_gettime_(&kernel_end);
				while(1) {
					int flag, type;
					MPI_Status stat;
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
							fprintf(stderr,"%d: recv(%d, %d) completed successfully\n", rank, stat.MPI_SOURCE, stat.MPI_TAG);
							break;
						}
						else {
//							fprintf(stderr,"%d: Busy waiting for recv\n", rank);
							usleep(10*1000); // 1/100th of a second
						}
					}
				}
				while(1) {
					int flag, type;
					MPI_Status stat;
					int tst = photon_test(sendReq, &flag, &type, &stat);
					if( tst < 0 ) {
						fprintf(stderr,"%d: An error occured in photon_test(send)\n", rank);
						exit(-1);
					}
					else if( tst > 0 ) {
						fprintf(stderr,"%d: That shouldn't have happened in this code\n", rank);
						exit(0);
					}
					else {
						if( flag ) {
							fprintf(stderr,"%d: send(%d, %d) completed successfully\n", rank, stat.MPI_SOURCE, stat.MPI_TAG);
							break;
						}
						else {
//							fprintf(stderr,"%d: Busy waiting for send\n", rank);
							usleep(10*1000); // 1/100th of a second
						}
					}
				}
				photon_gettime_(&total_end);
				overhead = (total_end-total_start) - (kernel_end-kernel_start);
				if (rank == 0 )
					printf("%i,%i,%i,%f,%f,%f\n",trial,arraySize,workSize,(total_end-total_start),(kernel_end-kernel_start),overhead);
			}
			photon_unregister_buffer(send,arraySize);
			photon_unregister_buffer(recv,arraySize);
			free(send);
			free(recv);
		}
	}
	photon_finalize();
	MPI_Finalize();
	return 0;
}

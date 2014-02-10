#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "photon.h"

#define PHOTON_RECV_SIZE 1024*1024*1024 // 1GB                                                                                                                
#define PHOTON_TAG       13

int main(int argc, char *argv[]) {
    uint32_t recvReq,sendReq;
    char *send,*recv;
    int rank,size,prev,next,fproc;

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);
    next = (rank+1) % size;
    prev = (size+rank-1) % size;
	fproc = size;

	char **forwarders = malloc(sizeof(char**));
	forwarders[0] = "b001/5006";

    struct photon_config_t cfg = {
		.meta_exch = PHOTON_EXCH_MPI,
		.nproc = size,
		.address = rank,
		.comm = MPI_COMM_WORLD,
		.use_forwarder = 1,
		.forwarder_eids = forwarders,
		.use_cma = 1,
		.eth_dev = "roce0",
		.ib_dev = "qib0",
		.ib_port = 1,
		.backend = "verbs"
    };
	
	photon_init(&cfg);

	recv = (char*)malloc(PHOTON_RECV_SIZE*sizeof(char));
	photon_register_buffer(recv, PHOTON_RECV_SIZE);
	
	int alloc_size = 1024*1024;
	struct photon_descriptor_t desc;
	desc.addr = (uintptr_t)0x00007f6605bff000;
	desc.size = alloc_size;
	desc.priv.key0 = 2576896;
	desc.priv.key1 = 2576896;

	/* rank=size,size+1,...size+n are the forwarders
	   need a proper mapping and lookup yet */
	photon_post_os_get_direct(fproc, recv, alloc_size, PHOTON_TAG, &desc, &recvReq);
	
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
				fprintf(stderr,"%d: recv(%d, %d) of size %u completed successfully\n", rank, (int)stat.src_addr, stat.tag, alloc_size);
				break;
			}
			else {
				//fprintf(stderr,"%d: Busy waiting for recv\n", rank);
				usleep(10*1000);
			}
		}
	}
	
	photon_finalize();

	MPI_Finalize();

	return 0;
}

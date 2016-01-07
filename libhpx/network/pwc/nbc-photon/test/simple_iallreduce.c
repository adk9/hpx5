#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <unistd.h>
#include <nbc.h>

static inline void  log_fail(char* msg){
  printf( "[Error! Test failed with msg : %s ]\n", msg);
  fflush(stdout);
}

static inline void  log_pass(char* msg){
  printf( "[Test passed with msg : %s ]\n", msg);
}

/*#define I 20*/
#define I 10000

int main(int argc, char **argv)
{
	char hostname[25];
	NBC_Comminfo comm  ;
        int val_reduce ;
        int val_result = -1;
        int val_expected;


	int  i, rank, wsize ;
	MPI_Status status;
	int root = 0;
	gethostname(hostname, 25);
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
	
	//DEBUG //todo remove
	if(0){
	  int i= 0;
 	  char hostname[256];
	  gethostname(hostname, sizeof(hostname));
	  printf("PID %d on %s rank :%d ready for attach\n", getpid(), hostname, rank);
	  fflush(stdout);
	  //special MPI depbug mode where we debug just one rank
	  //others will be running parallal after barrier 
	  if(rank == 0){
	    while (0 == i)
	     sleep(5);
	  }

	  MPI_Barrier(MPI_COMM_WORLD);
	}	
	//put rank as the value to reduce
	val_reduce = rank;
	//put expected value
	val_expected = (wsize / 2.0)* (wsize - 1);

        int active_ranks[wsize];
	for (i = 0; i < wsize; ++i) {
	  active_ranks[i] = i;	
	}

	printf("my hostname is : %s my rank is : %d \n", hostname, rank);
        int res = NBC_Init_comm(rank, active_ranks, wsize, wsize, &comm);
	if(res != NBC_OK){
	  printf( "Error NBC Initialization() from process %d \n", rank);
	  goto end;
	}

	for (i = 0; i < I; ++i) {
	  //photon version	
	  NBC_Handle handle;
	  NBC_Iallreduce(&val_reduce, &val_result, 1, MPI_INT, MPI_SUM, &comm, &handle); 
	  NBC_Wait(&handle);
	  
	  //MPI version
	  /*MPI_Request handle;*/
	  /*MPI_Status stat;*/
	  /*MPI_Iallreduce(&val_reduce, &val_result, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD, &handle); */
	  /*MPI_Wait(&handle, &stat);*/


	  int ret = (val_result != val_expected);
	  if(rank == root)
	    printf("allreduce result : %d  expected : %d  iteration : %d rank: %d \n", val_result, val_expected, i, rank);
	  if(ret){
	    printf("allreduce result : %d  expected : %d  iteration : %d rank: %d \n", val_result, val_expected, i, rank);
	    log_fail("expected value failed \n");	
	    goto end;  
	  }
	}
        
	printf( "Allreduce result from process %d : result : %d expected value : %d \n", rank, val_result, val_expected);
	if(rank == root){
	  sleep(1);	
	  log_pass(" All simple Allreduce tests were succesfull");
	}  
end:
	MPI_Barrier(MPI_COMM_WORLD);
	MPI_Finalize();
}


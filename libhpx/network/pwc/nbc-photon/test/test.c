#include <stdio.h>
#include <string.h>
#include <mpi.h>
#include <unistd.h>

char* my_msg =  "Helloabcdefghijklmno";

int main(int argc, char **argv)
{
	char hostname[256];
	char message[50];
	memset(message, '\0', sizeof(message));
	int  i, rank, wsize ;
	MPI_Status status;
	MPI_Request request = MPI_REQUEST_NULL;
	int root = 0;
	gethostname(hostname, 256);
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &wsize);
        int active_ranks[wsize];
	for (i = 0; i < wsize; ++i) {
	  active_ranks[i] = i;	
	}

	printf("my hostname is : %s my rank is : %d \n", hostname, rank);
	if (rank == root)
	{
		strcpy(message, my_msg); 
		printf("ROOT MSG : count [%d]  ptr: [%x]  msg : %s \n", (int)strlen(message), message, (char*)message);
	}
	
	printf("PROCESS MSG : count [%d]  ptr: [%x]  msg : %s \n", (int)strlen(message), message, (char*)message);


error:
	MPI_Finalize();
}


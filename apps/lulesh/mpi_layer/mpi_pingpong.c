#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mpi_wrapper.h"

int pingpong(int num_rounds, int data_size, void* data) {
  int rank;
  int size;
  int next, prev;
  MPI_Init(0, NULL);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;
  prev = (size+rank-1) % size;
  int buffer_size = data_size;


  char* send_buffer = NULL;
  char* recv_buffer = NULL;
  //  char* copy_buffer = NULL;
  MPI_Status recv_status;

  //send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  //copy_buffer = (char*)malloc(buffer_size*sizeof(char));
  send_buffer = (char*)data;

  for (int i = 0; i < num_rounds; i++) {
    //    printf("Beginning on rank %d\n", rank);
    fflush(NULL);
    if (rank == 0) {
      strcpy(send_buffer, "Message from proc 0"); 
      MPI_Send(data, buffer_size, MPI_CHAR, 1,
	       0, MPI_COMM_WORLD);
      //      printf("MPI_Send done  on rank %d\n", rank);
      fflush(NULL);
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR,
	       1, 0, MPI_COMM_WORLD, &recv_status);
      //      printf("MPI_Recv done  on rank %d\n", rank);
      fflush(NULL);
      //      printf("Received message from proc 1: %s\n", recv_buffer);
    }
    else if (rank == 1) {
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR,
	       0, 0, MPI_COMM_WORLD, &recv_status);
      //      printf("MPI_Recv done on rank %d\n", rank);
      //      fflush(NULL);
      int str_length;
      MPI_Send(data, buffer_size, MPI_CHAR, 0,
	       0, MPI_COMM_WORLD);
      //      printf("MPI_Send done on rank %d\n", rank);
      fflush(NULL);
    }
  }

  MPI_Finalize();

  free(recv_buffer);

  return 0;

}

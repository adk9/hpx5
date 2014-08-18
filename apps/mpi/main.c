#include <stdio.h>
#include <unistd.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"

static hpx_action_t _mpi = 0;
static hpx_action_t _hpxmain = 0;
extern hpx_time_t start_time;
hpx_time_t start_time;

void mpi_test_routine(int its)
{
  int numProcs,rank;
  MPI_Comm_size(MPI_COMM_WORLD_, &numProcs);
  MPI_Comm_rank(MPI_COMM_WORLD_, &rank) ;
  printf(" Number of procs %d rank %d\n",numProcs,rank);
  if ( numProcs < 2 ) {
    printf(" Need to have at least two persistent threads to test this\n");
    return;
  }

  int i,j;
  int buffsize = 20;
  double *sendbuff,*recvbuff;
  sendbuff=(double *)malloc(sizeof(double)*buffsize);
  recvbuff=(double *)malloc(sizeof(double)*buffsize);

  srand((unsigned)time( NULL ) + rank);
  for(i=0;i<buffsize;i++){
    sendbuff[i]=(double)rand()/RAND_MAX;
  }

  int taskid = rank;
  int ntasks = numProcs;
  double recvtime,totaltime;
  MPI_Status   status;
  MPI_Request	send_request,recv_request;
  int ierr,inittime,itask;
  double sendbuffsum,recvbuffsum;
  double *recvtimes, *sendbuffsums,*recvbuffsums;
  recvtimes=(double *)malloc(sizeof(double)*ntasks);
  sendbuffsums=(double *)malloc(sizeof(double)*ntasks);
  recvbuffsums=(double *)malloc(sizeof(double)*ntasks);
  int buffer[6];
  int receive_counts[4] = { 0, 1, 2, 3 };
  int receive_displacements[4] = { 0, 0, 1, 3 };
  int count = 1000;
  int *in, *out, *sol;
  int fnderr=0;
  in = (int *)malloc( count * sizeof(int) );
  out = (int *)malloc( count * sizeof(int) );
  sol = (int *)malloc( count * sizeof(int) );

  for (j=0;j<its;j++) {
    inittime = MPI_Wtime();
    // Example Isend/Irecv/Wait  
    if ( taskid == 0 ) {
      ierr=MPI_Isend(sendbuff,buffsize,MPI_DOUBLE,
	           taskid+1,0,MPI_COMM_WORLD_,&send_request);   
      ierr=MPI_Irecv(recvbuff,buffsize,MPI_DOUBLE,
	           ntasks-1,MPI_ANY_TAG_,MPI_COMM_WORLD_,&recv_request);
      recvtime = MPI_Wtime();
    } else if ( taskid == ntasks-1 ) {
      ierr=MPI_Isend(sendbuff,buffsize,MPI_DOUBLE,
	           0,0,MPI_COMM_WORLD_,&send_request);   
      ierr=MPI_Irecv(recvbuff,buffsize,MPI_DOUBLE,
	           taskid-1,MPI_ANY_TAG_,MPI_COMM_WORLD_,&recv_request);
      recvtime = MPI_Wtime();
    } else {
      ierr=MPI_Isend(sendbuff,buffsize,MPI_DOUBLE,
	           taskid+1,0,MPI_COMM_WORLD_,&send_request);
      ierr=MPI_Irecv(recvbuff,buffsize,MPI_DOUBLE,
	           taskid-1,MPI_ANY_TAG_,MPI_COMM_WORLD_,&recv_request);
      recvtime = MPI_Wtime();
    }
    ierr=MPI_Wait(&send_request,&status);
    ierr=MPI_Wait(&recv_request,&status);

    totaltime = MPI_Wtime() - inittime;

    recvbuffsum=0.0;
    for(i=0;i<buffsize;i++){
      recvbuffsum += recvbuff[i];
    }   
 
    ierr=MPI_Gather(&recvbuffsum,1,MPI_DOUBLE,
                   recvbuffsums,1, MPI_DOUBLE,
                   0,MPI_COMM_WORLD_);
 
    ierr=MPI_Gather(&recvtime,1,MPI_DOUBLE,
                   recvtimes,1, MPI_DOUBLE,
                   0,MPI_COMM_WORLD_);
    if ( taskid == 0 ) {
      for(itask=0;itask<ntasks;itask++){
        printf("Process %d: Sum of received vector= %e: Time=%f milliseconds\n",
               itask,recvbuffsums[itask],recvtimes[itask]);
      }  
      printf(" Communication time: %f seconds\n\n",totaltime);  
    }

    if (ntasks == 4 ) {
      if ( rank == 0 ) {
        printf(" Running gatherv test\n");
      }
      // Example gatherv
      for (i=0; i<rank; i++)
      {
        buffer[i] = rank;
      }
      MPI_Gatherv(buffer, rank, MPI_INT, buffer, receive_counts, receive_displacements, MPI_INT, 0, MPI_COMM_WORLD_);
      if ( rank == 0)
      {
        for (i=0; i<6; i++)
        {
            printf("[%d]", buffer[i]);
        }
        printf("\n");
        fflush(stdout);
      }
    }

    // Testing Allreduce
    for (i=0; i<count; i++)
    {
        *(in + i) = i;
        *(sol + i) = i*ntasks;
        *(out + i) = 0;
    }
    MPI_Allreduce( in, out, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD );
    for (i=0; i<count; i++)
    {
        if (*(out + i) != *(sol + i))
        {
            fnderr++;
        }
    }
    if (fnderr)
    {
        fprintf( stderr, "(%d) Error for type MPI_INT and op MPI_SUM fnderr %d\n", rank,fnderr );
        fflush(stderr);
    }
  }

  free(sendbuff);
  free(recvbuff);
  free(recvtimes);
  free(sendbuffsums);
  free(recvbuffsums);
  free( in );
  free( out );
  free( sol );
}

static int _mpi_action(int args[1] /* its */) {
  int err;
  start_time = hpx_time_now();
  mpi_init_(&err);
  mpi_test_routine(args[0]);
  mpi_finalize_(&err);
  return HPX_SUCCESS;
}

static int _hpxmain_action(int args[2] /* hpxranks, its */) {
  mpi_system_init(args[0], 0);

  hpx_addr_t and = hpx_lco_and_new(args[0]);
  int i,j,e;
  for (i = 0, e = hpx_get_num_ranks(); i < e; ++i) {
    int size = args[0];
    int ranks_there = 0; // important to be 0
    get_ranks_per_node(i, &size, &ranks_there, NULL);

    
    for (j = 0; j < ranks_there; ++j)
      hpx_call(HPX_THERE(i), _mpi, &args[1], sizeof(int), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  mpi_system_shutdown();
  hpx_shutdown(0);
}

int main(int argc, char *argv[])
{

   char hostname[256];
   gethostname(hostname, sizeof(hostname));
   printf("PID %d on %s ready for attach\n", getpid(), hostname);
   fflush(stdout);
   //   sleep(8);

  if ( argc < 4 ) {
    printf(" Usage: test <number of OS threads> <# of persistent hpx threads> <# of iterations>\n");
    exit(0);
  }

  uint64_t numos = atoll(argv[1]);
  int numhpx = atoi(argv[2]);
  int its = atoi(argv[3]);

  printf(" Number OS threads: %ld Number persistent lightweight threads: %d its: %d\n",numos,numhpx,its);

  hpx_config_t cfg = {
    .cores = numos,
    .threads = numos,
    //    .stack_bytes = 2<<24
    .gas = HPX_GAS_PGAS
  };

  int error = hpx_init(&cfg);
  if (error != HPX_SUCCESS)
    exit(-1);

  mpi_system_register_actions();

  // register local actions
  _mpi = HPX_REGISTER_ACTION(_mpi_action);
  _hpxmain = HPX_REGISTER_ACTION(_hpxmain_action);

  // do stuff here
  int args[2] = { numhpx, its };
  hpx_run(_hpxmain, args, sizeof(args));

  return 0;
}

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

int mpi_test_routine(int its)
{
  int numProcs,rank;
  MPI_Comm_size(MPI_COMM_WORLD_, &numProcs);
  MPI_Comm_rank(MPI_COMM_WORLD_, &rank) ;
  printf(" Number of procs %d rank %d\n",numProcs,rank);
  if ( numProcs < 2 ) {
    printf(" Need to have at least two persistent threads to test this\n");
    return 1;
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
  MPI_Request   send_request,recv_request;
  int ierr = 0,inittime,itask;
  double recvbuffsum;
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
  int chunk = 128;
  int *sb,*rb;
  sb = (int *)malloc(ntasks*chunk*sizeof(int));
  rb = (int *)malloc(ntasks*chunk*sizeof(int));
  int *sendcounts, *recvcounts, *rdispls, *sdispls;
  int *sbuf, *rbuf;
  int *p;
  int err;
  int errs;
  int *sendbuf, *recvbuf;
  int root;
  sendbuf = (int *)malloc( count * sizeof(int) );
  recvbuf = (int *)malloc( count * sizeof(int) );

  sbuf = (int *)malloc( ntasks * ntasks * sizeof(int) );
  rbuf = (int *)malloc( ntasks * ntasks * sizeof(int) );

  sendcounts = (int *)malloc( ntasks * sizeof(int) );
  recvcounts = (int *)malloc( ntasks * sizeof(int) );
  rdispls = (int *)malloc( ntasks * sizeof(int) );
  sdispls = (int *)malloc( ntasks * sizeof(int) );

  int *displs,*send_counts,*row,**table;
  int errors = 0;
  displs = (int *)malloc( ntasks*sizeof(int));
  send_counts = (int *)malloc( ntasks*sizeof(int));
  row = (int *)malloc( ntasks*sizeof(int));
  table = (int **)malloc( ntasks*sizeof(int *));
  for (i=0;i<ntasks;i++) {
    table[i] = (int *) malloc (ntasks*sizeof(int));
  }
  int recv_count;

  int left,right;
  int *srbuffer,*srbuffer2;
  srbuffer = (int *) malloc(10*sizeof(int));
  srbuffer2 = (int *) malloc(10*sizeof(int));
  MPI_Status srstatus;

  for (j=0;j<its;j++) {
    inittime = MPI_Wtime();
    if ( rank == 0 ) printf(" Beginning Isend/Irecv/Wait/Gather Test\n");
    // Example Isend/Irecv/Wait/Gather  -------------------------------------------------
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
    if ( rank == 0 )printf(" Finished Isend/Irecv/Wait/Gather Test\n");

    if ( rank == 0 )printf(" Beginning Sendrecv Test\n");
    // Example sendrecv -------------------------------------------------
    right = (rank + 1) % ntasks;
    left = rank - 1;
    if (left < 0)
        left = ntasks - 1;

    for (i=0;i<6;i++) {
      buffer[i] = rank*100 + i;
    }

    MPI_Sendrecv(srbuffer, 10, MPI_INT, left, 123, srbuffer2, 10, MPI_INT, right, 123, MPI_COMM_WORLD_, &srstatus);

    if ( rank == 0 )printf(" Finished Sendrecv Test\n");

    // Example gatherv -------------------------------------------------
    if (ntasks == 4 ) {
      if ( rank == 0 )printf(" Beginning Gatherv Test\n");
      if ( rank == 0 ) {
        printf(" Running gatherv test\n");
      }
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
      if ( rank == 0 )printf(" Finished Gatherv Test\n");
    } else {
      if ( rank == 0 )printf(" Skipping Gatherv Test\n");
    }

    if ( rank == 0 )printf(" Beginning Allreduce Test\n");
    // Testing Allreduce -------------------------------------------------
    for (i=0; i<count; i++)
    {
        *(in + i) = i;
        *(sol + i) = i*ntasks;
        *(out + i) = 0;
    }
    MPI_Allreduce( in, out, count, MPI_INT, MPI_SUM, MPI_COMM_WORLD_ );
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
    if ( rank == 0 )printf(" Finished Allreduce Test\n");

    if ( rank == 0 )printf(" Beginning Bcast Test\n");
    // Testing Bcast -------------------------------------------------
    if (rank == 0 ) {
      buffer[5] = 6;
    } else {
      buffer[5] = 0;
    }
    MPI_Bcast(buffer, 6, MPI_INT, 0, MPI_COMM_WORLD_);
    if ( buffer[5] != 6 ) {
      fprintf( stderr, "(%d) Error for type MPI_INT MPI_Bcast\n", rank);
      fflush(stderr);
    }
    if ( rank == 0 )printf(" Finished Bcast Test\n");

    if ( rank == 0 )printf(" Beginning Alltoall Test\n");
    // Testing Alltoall------------------------------------------------
    for ( i=0 ; i < ntasks*chunk ; ++i ) {
        sb[i] = rank + 1;
        rb[i] = 0;
    }
    status = MPI_Alltoall(sb, chunk, MPI_INT, rb, chunk, MPI_INT, MPI_COMM_WORLD_);

    if ( ntasks > 1 ) {
      if (rb[chunk] != 2 ) {
        fprintf( stderr, "(%d) Error for type Alltoall\n", rank);
        fflush(stderr);
      }
    }
    if ( rank == 0 )printf(" Finished Alltoall Test\n");

    if ( rank == 0 )printf(" Beginning Reduce Test\n");
    // Testing reduce------------------------------------------------
    errs = 0;
    for (root = 0; root < ntasks; root ++) {
      for (i=0; i<count; i++) sendbuf[i] = i;
      for (i=0; i<count; i++) recvbuf[i] = -1;
      MPI_Reduce( sendbuf, recvbuf, count, MPI_INT, MPI_SUM, root, MPI_COMM_WORLD_ );
      if (rank == root) {
        for (i=0; i<count; i++) {
          if (recvbuf[i] != i * ntasks) {
            errs++;
          }
        }
      }
    }
    if ( errs > 0 ) printf(" (%d) Error to type MPI_Reduce MPI_INT MPI_SUM %d\n",errs,rank);
    if ( rank == 0 )printf(" Finished Reduce Test\n");


    if ( rank == 0 )printf(" Beginning Scatterv Test\n");
    // Testing reduce------------------------------------------------
    errors = 0;
    recv_count = ntasks;
    if (rank == 0)
      for ( i=0; i<ntasks; i++) {
        send_counts[i] = recv_count;
        displs[i] = i * ntasks;
        for ( j=0; j<ntasks; j++ )
          table[i][j] = i+j;
      }

    /* Scatter the big table to everybody's little table */
    MPI_Scatterv(&table[0][0], send_counts, displs, MPI_INT,
                     &row[0] , recv_count, MPI_INT, 0, MPI_COMM_WORLD_);

    /* Now see if our row looks right */
    for (i=0; i<ntasks; i++)
      if ( row[i] != i+rank ) errors++;

    if ( errors > 0 ) printf(" (%d) Error to type MPI_Scatterv %d\n",errors,rank);
    if ( rank == 0 )printf(" Finished Scatterv Test\n");



    if ( rank == 0 )printf(" Beginning Alltoallv Test\n");
    // Testing Alltoallv------------------------------------------------
    for (i=0; i<ntasks*ntasks; i++) {
        sbuf[i] = i + 100*rank;
        rbuf[i] = -i;
    }
    for (i=0; i<ntasks; i++) {
        sendcounts[i] = i;
        recvcounts[i] = rank;
        rdispls[i] = i * rank;
        sdispls[i] = (i * (i+1))/2;
    }
    MPI_Alltoallv( sbuf, sendcounts, sdispls, MPI_INT,
                       rbuf, recvcounts, rdispls, MPI_INT, MPI_COMM_WORLD_ );
    /* Check rbuf */
    err = 0;
    for (i=0; i<ntasks; i++) {
        p = rbuf + rdispls[i];
        for (j=0; j<rank; j++) {
            if (p[j] != i * 100 + (rank*(rank+1))/2 + j) {
                fprintf( stderr, "[%d] got %d expected %d for %dth\n",
                                    rank, p[j],(i*(i+1))/2 + j, j );
                fflush(stderr);
                err++;
            }
        }
    }
    if ( rank == 0 )printf(" Finished Alltoallv Test\n");

  }

  free(srbuffer);
  free(srbuffer2);
  free(row);
  free(displs);
  free(send_counts);
  for (i=0;i<ntasks;i++) {
    free(table[i]);
  }
  free(table);
  free(sendbuf);
  free(recvbuf);
  free(sbuf);
  free(rbuf);
  free(sdispls);
  free(rdispls);
  free(recvcounts);
  free(sendcounts);
  free(sb);
  free(rb);
  free(sendbuff);
  free(recvbuff);
  free(recvtimes);
  free(sendbuffsums);
  free(recvbuffsums);
  free( in );
  free( out );
  free( sol );
  return ierr;
}

static int _mpi_action(int args[1] /* its */) {
  int err;
  start_time = hpx_time_now();
  mpi_init_(&err);
  err = mpi_test_routine(args[0]);
  mpi_finalize_(&err);
  if (err)
    return HPX_ERROR;
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

  int error = hpx_init(&argc, &argv);
  if (error != HPX_SUCCESS)
    exit(-1);

  if ( argc < 3 ) {
    printf(" Usage: test <# of persistent hpx threads> <# of iterations>\n");
    exit(0);
  }

  int numhpx = atoi(argv[1]);
  int its = atoi(argv[2]);

  printf("Number persistent lightweight threads: %d its: %d\n",numhpx,its);

  mpi_system_register_actions();

  // register local actions
  HPX_REGISTER_ACTION(_mpi_action, &_mpi);
  HPX_REGISTER_ACTION(_hpxmain_action, &_hpxmain);

  // do stuff here
  int args[2] = { numhpx, its };
  hpx_run(&_hpxmain, args, sizeof(args));

  return 0;
}

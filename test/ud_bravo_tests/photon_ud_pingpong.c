#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "photon.h"
#include "bravo_ids.h"

#define PHOTON_TAG  13
#define PING         0
#define PONG         1

#define SUBTRACT_TV(E,S) (((E).tv_sec - (S).tv_sec) + ((E).tv_usec - (S).tv_usec)/1e6)
//#define dbg_printf(format, args...) fprintf(stderr, format, ## args);
#define dbg_printf(format, args...)

enum test_type {PHOTON_TEST,
                PHOTON_UD_TEST,
                MPI_TEST,
};

int pp_test = PHOTON_TEST;
int global_iters = 100;
int rank, other_rank, size;

struct pingpong_args {
  int type;
  int ping_id;
  int pong_id;
  char msg[1024];
};

struct pingpong_args *send_args;
struct pingpong_args *recv_args;

bravo_node *dnode = NULL;
bravo_node *mynode = NULL;
photon_addr naddr;

int send_pingpong(int dst, int ping_id, int pong_id, int pp_type) {
  struct timeval start, end;
  uint32_t send_req;

  send_args->type = pp_type;
  send_args->ping_id = ping_id;
  send_args->pong_id = pong_id;
  send_args->msg[0] = '\0';

  if (pp_test == PHOTON_TEST) {
    //photon_post_send_buffer_rdma(dst, (char*)send_args, sizeof(*send_args), PHOTON_TAG, &send_req);
    //gettimeofday(&start, NULL);
    photon_wait_recv_buffer_rdma(dst, PHOTON_TAG, &send_req);
    //gettimeofday(&end, NULL);
    //if (rank == 0)
    //  printf("%d: wait_recv time: %f\n", rank, SUBTRACT_TV(end, start));
    
    //gettimeofday(&start, NULL);
    photon_post_os_put(send_req, dst, (void*)send_args, sizeof(*send_args), PHOTON_TAG, 0);
    //gettimeofday(&end, NULL);
    //if (rank == 0)
    //  printf("%d: os_put time: %f\n", rank, SUBTRACT_TV(end, start));
    while (1) {
      int flag, type;
      struct photon_status_t stat;
      photon_test(send_req, &flag, &type, &stat);
      if(flag) {
        dbg_printf("%d: send_pingpong(%d->%d)[%d] of size %lu completed successfully\n",
                   rank, rank, dst, pp_type, sizeof(*send_args));
        break;
      }
      else {
        //usleep(10);
      }
    }
    //gettimeofday(&start, NULL);
    photon_send_FIN(send_req, dst);
    //gettimeofday(&end, NULL);
    //if (rank == 0)
    //  printf("%d: send_FIN time: %f\n", rank, SUBTRACT_TV(end, start));
  }
  else if (pp_test == PHOTON_UD_TEST) {
    uint64_t send_req;

    //gettimeofday(&start, NULL);
    photon_send(&dnode->mgid, (void*)send_args, sizeof(*send_args), 0, &send_req);
    //gettimeofday(&end, NULL);
    //if (rank == 0)
    //  printf("%d: send time: %f\n", rank, SUBTRACT_TV(end, start));
    //gettimeofday(&start, NULL);
    while (1) {
      int flag, type;
      struct photon_status_t stat;
      photon_test(send_req, &flag, &type, &stat);
      if(flag) {
        dbg_printf("%d: send(%d->%d)[%d] ping id=%d of size %lu completed successfully\n",
                   rank, mynode->index+1, dnode->index+1, pp_type, send_args->ping_id, sizeof(*send_args));
        break;
      }
      else {
        //usleep(10);
      }
    }
    //gettimeofday(&end, NULL);
    //if (rank == 0)
    //  printf("%d: send_wait time: %f\n", rank, SUBTRACT_TV(end, start));
  }
  else if (pp_test == MPI_TEST) {
    MPI_Request mpi_r;
    MPI_Status stat;
    int flag;
    MPI_Isend((void*)send_args, sizeof(*send_args), MPI_BYTE, dst, rank, MPI_COMM_WORLD, &mpi_r);
    //MPI_Wait(&mpi_r, &stat);
    while (1) {
      MPI_Test(&mpi_r, &flag, &stat);
      if (flag) {
        break;
      }
      else {
        //usleep(10);
      }
    }
  }

  return 0;
}

void *receiver(void *args) {
  struct timeval start, end;

  while (1) {
    uint32_t recv_req;

    /* check if we have something to receive first, mimicking HPX parcel handler
       instead of blocking in wait_send_buffer... */
    /*
    if (pp_test == PHOTON_TEST) {
      struct photon_status_t stat;
      int flag, ret;
      do {
        ret = photon_probe_ledger(other_rank, &flag, PHOTON_SEND_LEDGER, &stat);
      } while (flag != 1);
    }
    else if (pp_test == MPI_TEST) {
      MPI_Status stat;
      int flag, ret;
      do {
        ret = MPI_Iprobe(other_rank, other_rank, MPI_COMM_WORLD, &flag, &stat);
      } while (flag != 1);
    }
    */

    if (pp_test == PHOTON_TEST) {
      //photon_wait_send_buffer_rdma(other_rank, PHOTON_TAG, &recv_req);
      //photon_post_os_get(recv_req, other_rank, (void*)recv_args, sizeof(*recv_args), PHOTON_TAG, 0);
      //gettimeofday(&start, NULL);
      photon_post_recv_buffer_rdma(other_rank, (void*)recv_args, sizeof(*recv_args), PHOTON_TAG, &recv_req);
      while (1) {
        int flag, type;
        struct photon_status_t stat;
        photon_test(recv_req, &flag, &type, &stat);
        if(flag) {
          dbg_printf("%d: recv(%d<-%d) of size %lu completed successfully\n",
                     rank, rank, (int)stat.src_addr.global.proc_id, sizeof(*args));
          break;
        }
        else {
          //usleep(10);
        }
      }
      //gettimeofday(&end, NULL);
      //printf("%d: post_recv time: %f\n", rank, SUBTRACT_TV(end, start));
      //photon_send_FIN(recv_req, other_rank);
    }
    else if (pp_test == PHOTON_UD_TEST) {
      int flag;
      struct photon_status_t stat;
      //gettimeofday(&start, NULL);
      while (1) {
        photon_addr addr = {.s_addr = 0};
        photon_probe(&addr, &flag, &stat);
        if (flag) {
          break;
        }
      }
      //gettimeofday(&end, NULL);
      //if (rank == 0)
      //  printf("%d: probe time: %f\n", rank, SUBTRACT_TV(end, start));
      //gettimeofday(&start, NULL);
      photon_recv(stat.request, recv_args, stat.size, 0);
      //gettimeofday(&end, NULL);
      //if (rank == 0)
      //  printf("%d: recv time: %f\n", rank, SUBTRACT_TV(end, start));
      dbg_printf("%d: recv(%d<-%d) of size %lu completed successfully\n",
                 rank, mynode->index+1, dnode->index+1, stat.size);
    }
    else if (pp_test == MPI_TEST) {
      MPI_Request mpi_r;
      MPI_Status stat;
      int flag;
      MPI_Irecv((void*)recv_args, sizeof(*recv_args), MPI_BYTE, other_rank, other_rank, MPI_COMM_WORLD, &mpi_r);
      //MPI_Wait(&mpi_r, &stat);
      while (1) {
        MPI_Test(&mpi_r, &flag, &stat);
        if (flag) {
          break;
        }
        else {
          //usleep(10);
        }
      }
    }

    switch(recv_args->type) {
    case PING:
      dbg_printf("%d: got ping %d from %d, sending pong back...\n", rank, recv_args->ping_id, other_rank);
      send_pingpong(other_rank, -1, recv_args->ping_id, PONG);
      if (recv_args->ping_id == (global_iters-1)) {
        return NULL;
      }
      break;
    case PONG:
      dbg_printf("%d: got pong %d from %d, sending ping back...\n", rank, recv_args->pong_id, other_rank);
      if (recv_args->pong_id == (global_iters-1)) {
        return NULL;
      }
      send_pingpong(other_rank, recv_args->pong_id+1, -1, PING);
      break;
    default:
      fprintf(stderr, "%d: Unknown pingpong type\n", rank);
      exit(-1);
      break;
    }
  }

  return NULL;
}

int main(int argc, char **argv) {
  char buf[INET_ADDRSTRLEN];
  char *test = "PHOTON";
  pthread_t th;
  struct timeval start, end;

  if (argc > 2) {
    test = strdup(argv[1]);
    global_iters = atoi(argv[2]);
  }
  if (global_iters < 1) {
    printf("Usage: %s <type> <iters>\n", argv[0]);
    exit(-1);
  }

  if (!strcasecmp(test, "PHOTON"))
    pp_test = PHOTON_TEST;
  else if (!strcasecmp(test, "UD"))
    pp_test = PHOTON_UD_TEST;
  else if (!strcasecmp(test, "MPI"))
    pp_test = MPI_TEST;

  MPI_Init(&argc,&argv);
  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  other_rank = (rank+1) % size;

  struct photon_config_t cfg = {
    .meta_exch = PHOTON_EXCH_MPI,
    .nproc = size,
    .address = rank,
    .comm = MPI_COMM_WORLD,
    .use_forwarder = 0,
    .use_cma = 0,
    .use_ud = 1,
    .eth_dev = "roce0",
    .ib_dev = "mlx4_1",
    .ib_port = 1,
    .backend = "verbs"
  };

  photon_init(&cfg);

  send_args = malloc(sizeof(struct pingpong_args));
  recv_args = malloc(sizeof(struct pingpong_args));
  
  photon_get_dev_addr(AF_INET, &naddr);
  
  inet_ntop(AF_INET, &naddr.s_addr, buf, sizeof(buf));
  printf("%d: dev addr: %s\n", rank, buf);

  if (pp_test == PHOTON_TEST) {
    photon_register_buffer((char*)send_args, sizeof(*send_args));
    photon_register_buffer((char*)recv_args, sizeof(*recv_args));
  }

  if (pp_test == PHOTON_UD_TEST) {
    init_bravo_ids();
    mynode = find_bravo_node(&naddr);
    
    if (!mynode) {
      printf("could not find my node\n");
      return 1;
    }
    
    switch (mynode->index) {
    case B005:
      dnode = get_bravo_node(B006);
      break;
    case B006:
      dnode = get_bravo_node(B005);
      break;
    default:
      break;
    }
    
    if (!dnode) {
      printf("could not find dest node\n");
      return 1;
    } 
    
    photon_register_addr(&mynode->mgid, AF_INET6);
  }

  // get everyone initialized before we start sending
  MPI_Barrier(MPI_COMM_WORLD);
  
  gettimeofday(&start, NULL);

  if (rank == 0) {
    send_pingpong(other_rank, 0, -1, PING);
  }

  pthread_create(&th, NULL, receiver, NULL);
  pthread_join(th, NULL);

  MPI_Barrier(MPI_COMM_WORLD);

  gettimeofday(&end, NULL);

  if (rank == 0)
    printf("%d: total time: %f\n", rank, SUBTRACT_TV(end, start));

  if (pp_test == PHOTON_TEST || pp_test == PHOTON_UD_TEST)
    photon_finalize();
  
  MPI_Finalize();

  return 0;
}

#include <stdio.h>
#include <unistd.h>

#include <hpx/hpx.h>
#include <libsync/sync.h>

#include "mpi_wrapper.h"
#include "mpi_system.h"
#include "timer.h"

int pingpong();
hpx_action_t action_pingpong;

struct hpxmain_args {
  int num_ranks;
  int num_rounds;
  hpx_action_t action;
};

struct pingpong_args {
  int num_rounds;
  int data_size;
  void* data;
};

int pingpong_action(struct pingpong_args* args) {
  pingpong(args->num_rounds, args->data_size, args->data);
  return 0;
}

int hpxmain_action(void *vargs) {
  struct hpxmain_args *args = (struct hpxmain_args*)vargs;
  int num_hpx_ranks = hpx_get_num_ranks();

  hpx_addr_t done_futs[args->num_ranks];

  mpi_system_init(args->num_ranks, 0);

  hpx_timer_t ts;
  hpx_get_time(&ts);

  struct pingpong_args pingpong_args;
  pingpong_args.num_rounds = args->num_rounds;
  pingpong_args.data_size = 100000;
  pingpong_args.data = malloc(pingpong_args.data_size);

  int k = 0;
  for (int i = 0; i < 2; i++) {
    done_futs[k] = hpx_lco_future_new(0);
    hpx_call(HPX_THERE(get_hpx_rank_from_mpi_rank(i)), args->action, (void*)&pingpong_args, sizeof(pingpong_args), done_futs[k]);
    k++;
  }
  for (int i = 0; i < 2; i++)
    hpx_lco_get(done_futs[i], NULL, 0);
  // hpx_lco_get_all(2, done_futs, NULL, 0); // this does not work

  double elapsed = hpx_elapsed_us(ts);
  printf("elapsed = %g\n", elapsed);
  printf("per = %g\n", elapsed/(args->num_rounds*2));


  free(pingpong_args.data);
  for (int i = 0; i < 2; i++)
    hpx_lco_delete(done_futs[i], HPX_NULL);

  hpx_shutdown(0);
  return HPX_SUCCESS;
}

hpx_timer_t ts;

void start_time()
{
  hpx_get_time(&ts);
}

double etime()
{
  double elapsed = hpx_elapsed_us(ts);
  return elapsed;
}

void mpi_barrier_( MPI_Comm *pcomm,int *pier)
{
}

int main(int argc, char *argv[])
{

  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname); 
  //fflush(stdout);
  //  sleep(8);

  hpx_config_t cfg = {0, 0, 0};

  if ( argc < 3 ) {
    printf(" Usage: test <number of OS threads> <number of rounds>\n");
    exit(0);
  }
  uint64_t numos = atoi(argv[1]);
  //  int numhpx = atoi(argv[2]);
  int num_rounds = atoi(argv[2]);

  int numhpx = 2;
  printf(" Number OS threads: %ld Number lightweight threads: %d\n",numos, numhpx);

  cfg.cores = numos; /* 0 = all available (default) */
  //  int stacksize = 2<<24; //2<<28;
  //  cfg.stack_bytes = stacksize;
  int error = hpx_init(&cfg);
  if (error != HPX_SUCCESS)
    exit(-1);

  mpi_system_register_actions();
  
  // do stuff here
  
  hpx_action_t hpxmain = hpx_register_action("hpxmain", (hpx_action_handler_t)hpxmain_action);
  action_pingpong = hpx_register_action("pingpong", (hpx_action_handler_t)pingpong_action);

  struct hpxmain_args args;
  args.num_ranks = 2;
  args.num_rounds = num_rounds;
  args.action = action_pingpong;
  hpx_run(hpxmain, &args, sizeof(args));

  return 0;

}

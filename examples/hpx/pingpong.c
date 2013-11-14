#include <string.h>
#include <stdio.h>

#include "hpx.h"

#define BUFFER_SIZE 128

int other_rank;
int global_count = 0;
hpx_locality_t* my_loc;
hpx_locality_t* other_loc;
hpx_future_t done_fut;
hpx_action_t* a_done;

int opt_iter_limit = 1000;
int opt_text_ping = 0;
int opt_screen_out = 0;

static hpx_action_t ping_action = HPX_ACTION_NULL;
static hpx_action_t pong_action = HPX_ACTION_NULL;
static hpx_action_t done_action = HPX_ACTION_NULL;

static void
done(void* args)
{
  hpx_lco_future_set_state(&done_fut);
}

typedef struct {
  int ping_id;
  int pong_id;
  char msg[128];
} pingpong_args_t;

static void
ping(pingpong_args_t* args)
{
  int ping_id = args->pong_id + 1;

  if (ping_id >= opt_iter_limit) {
    hpx_parcel_t *p = hpx_parcel_acquire(0);
    hpx_parcel_set_action(p, done_action);
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
    hpx_action_invoke(done_action, NULL, NULL);
  }
  else {
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
    if (!p) {
      fprintf(stderr, "Failed to acquire parcel in 'ping' action, aborting!\n");
      hpx_abort();
    }
    /* set pong as target action */
    hpx_parcel_set_action(p, pong_action);
    /* fill in pong arguments */
    pingpong_args_t *pargs = (pingpong_args_t*)hpx_parcel_get_data(p);
    pargs->ping_id = ping_id;
    pargs->msg[0]  = '\0';
    if (opt_text_ping)
      snprintf(pargs->msg, 128, "Message %d from proc 0", ping_id);
    if (opt_screen_out)
      printf("Ping acting; global_count=%d, message=%s\n", global_count, pargs->msg);    
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
  }
  
  global_count++;
}

static void
pong(pingpong_args_t* args)
{
  if (args->ping_id >= opt_iter_limit) {
  }
  else {
    int pong_id = args->ping_id + 1;
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
    if (!p) {
      fprintf(stderr, "Could not allocate parcel in 'pong' action, aborting!\n");
      hpx_abort();
    }
    hpx_parcel_set_action(p, ping_action);
    pingpong_args_t* out_args = (pingpong_args_t*)hpx_parcel_get_data(p);
    out_args->pong_id = pong_id;
    out_args->msg[0] = '\0';
    if (opt_text_ping) {
      int str_length;
      char copy_buffer[BUFFER_SIZE];
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", pong_id);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], args->msg);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(out_args->msg, copy_buffer);
    }
    
    if (opt_screen_out)
      printf("Pong acting; global_count=%d, message=%s\n", global_count, out_args->msg);
    hpx_parcel_send(other_loc, p, NULL, NULL, NULL);
    hpx_parcel_release(p);
  }

  global_count++;
}

static void
pingpong(void* _args)
{
  unsigned int my_rank = my_loc->rank;
  hpx_lco_future_init(&done_fut);

  if (my_rank == 0) {
    pingpong_args_t *args = hpx_alloc(sizeof(*args));
    args->pong_id = -1;
    //    p = hpx_alloc(sizeof(hpx_parcel_t));
    //    success = hpx_new_parcel("_ping_action", (void*)args, sizeof(struct pingpong_args), p);
    //    success = hpx_send_parcel(other_loc, p);
    hpx_action_invoke(ping_action, args, NULL);
  }
  else if (my_rank == other_rank) {
  }
  else {
  }
  hpx_thread_wait(&done_fut);
  hpx_locality_destroy(other_loc);
}

int main(int argc, char** argv)
{
  int success;
  hpx_future_t* fut;
  
  if (argc > 1)
    opt_iter_limit = atoi(argv[1]);
  if (opt_iter_limit < 0) {
    printf("Bad iteration limit\n");
    exit(-1);
  }
  if (argc > 2)
    opt_text_ping = atoi(argv[2]);
  if (argc > 3)
    opt_screen_out = atoi(argv[3]);
  printf("Running with options: {iter limit: %d}, {text_ping: %d}, {screen_out: %d}\n", opt_iter_limit, opt_text_ping, opt_screen_out);

  success = hpx_init();
  if (success != 0)
    exit(-1);

  /* I'm cheating by putting this before action registrations to make sure this gets done at all processes... */
  unsigned int num_ranks;
  num_ranks = hpx_get_num_localities();
  other_rank = num_ranks - 1;
  printf("Running pingpong on %d ranks between rank 0 and rank %d\n", num_ranks, other_rank);
  my_loc = hpx_get_my_locality();
  int my_rank = my_loc->rank;
  if (my_rank == 0)
    other_loc =  hpx_locality_from_rank(other_rank);
  else if (my_rank == other_rank)
    other_loc =  hpx_locality_from_rank(0);
  else 
    {}
  
  /* register action for parcel (must be done by all ranks) */
  ping_action = hpx_action_register("_ping_action", (hpx_func_t)ping);
  pong_action = hpx_action_register("_pong_action", (hpx_func_t)pong);
  done_action = hpx_action_register("_done_action", (hpx_func_t)done);
  hpx_action_registration_complete();

  hpx_timer_t ts;
  hpx_get_time(&ts);
  hpx_thread_create(__hpx_global_ctx, 0, (hpx_func_t)pingpong, 0, &fut, NULL);
  hpx_thread_wait(fut);
  double elapsed = hpx_elapsed_us(ts);
  double avg_oneway_latency = elapsed/((double)(opt_iter_limit*2));
  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);
  
  hpx_cleanup();

  return 0;
}

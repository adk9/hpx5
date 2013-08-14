#include <string.h>
#include <stdio.h>

#include "hpx.h"

struct pingpong_args {
  int ping_id;
  int pong_id;
  char msg[128];
};

#define BUFFER_SIZE 128
int iter_limit = 10;
int global_count = 0;
hpx_locality_t* my_loc;
hpx_locality_t* other_loc;

void _ping_action(void* _args) {
  if (_args == NULL) {
    printf("Dieing horribly - bad args!\n");
    exit(-1);
  }

  int success;
  hpx_parcel_t* p;
  struct pingpong_args* in_args = (struct pingpong_args*)_args;
  int ping_id = in_args->pong_id + 1;
  struct pingpong_args* out_args;

  out_args = hpx_alloc(sizeof(struct pingpong_args));
  if (out_args == NULL) {
    printf("Dieing horribly - no memory!\n");
    exit(-1);
  }
  out_args->ping_id = ping_id;
  snprintf(out_args->msg, 128, "Message %d from proc 0", ping_id);

  printf("Ping acting; global_count=%d, message=%s\n", global_count+1, out_args->msg);

  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel("_pong_action", (void*)out_args, sizeof(struct pingpong_args), p);
  success = hpx_send_parcel(other_loc, p);

  hpx_free(in_args);
  global_count++;
}

void _pong_action(void* _args) {
  if (_args == NULL) {
    printf("Dieing horribly - bad args!\n");
    exit(-1);
  }

  int success;
  int str_length;
  char copy_buffer[BUFFER_SIZE];
  hpx_parcel_t* p;
  struct pingpong_args* out_args;
  struct pingpong_args* in_args = (struct pingpong_args*)_args;
  int ping_id = in_args->ping_id;
  int pong_id = ping_id;

  out_args = hpx_alloc(sizeof(struct pingpong_args));
  if (out_args == NULL) {
    printf("Dieing horribly!\n");
    exit(-1);
  }
  out_args->pong_id = pong_id;

  snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", pong_id);
  str_length = strlen(copy_buffer);
  strcpy(&copy_buffer[str_length], in_args->msg);
  str_length = strlen(copy_buffer);
  strcpy(&copy_buffer[str_length], "'");
  strcpy(out_args->msg, copy_buffer);

  printf("Pong acting; global_count=%d, message=%s\n", global_count+1, out_args->msg);

  p = hpx_alloc(sizeof(hpx_parcel_t));
  success = hpx_new_parcel("_ping_action", (void*)out_args, sizeof(struct pingpong_args), p);
  success = hpx_send_parcel(other_loc, p);

  global_count++;
}

int main() {
  int success;

  /* DEBUG CODE */

  int i = 0;
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("PID %d on %s ready for attach\n", getpid(), hostname);
  fflush(stdout);
  sleep(15);
  /* END DEBUG CODE */


  struct timespec begin_ts;
  struct timespec end_ts;
  unsigned long elapsed;
  double avg_oneway_latency;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_parcel_t* p;
  struct pingpong_args* args;
  hpx_action_t* a_ping;
  hpx_action_t* a_pong;

  hpx_init();

  num_ranks = hpx_get_num_localities();
  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  /* register action for parcel (must be done by all ranks) */
  a_ping = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_ping_action", (hpx_func_t)_ping_action, a_ping);
  a_pong = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_pong_action", (hpx_func_t)_pong_action, a_pong);

  if (my_rank == 0)
    other_loc = hpx_get_locality(1);
  else if (my_rank == 1)
    other_loc = hpx_get_locality(0);
  else 
    {}


  clock_gettime(CLOCK_MONOTONIC, &begin_ts);
  if (my_rank == 0) {
    args = hpx_alloc(sizeof(struct pingpong_args));
    args->ping_id = 0;
    //    p = hpx_alloc(sizeof(hpx_parcel_t));
    //    success = hpx_new_parcel("_ping_action", (void*)args, sizeof(struct pingpong_args), p);
    //    success = hpx_send_parcel(other_loc, p);
    int *result = hpx_alloc(sizeof(int));
    hpx_action_invoke(a_ping, args, (void**)&result);
    while (global_count < iter_limit) {
    }
    free(result);
  }
  else if (my_rank ==1) {
    while (global_count < iter_limit-1) {
    }
  }
  else
    {}

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);
  
  avg_oneway_latency = elapsed/((double)(iter_limit*2));
  
  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);
  
  hpx_locality_destroy(other_loc);

  free(a_ping);
  free(a_pong);
  
  hpx_cleanup();
}

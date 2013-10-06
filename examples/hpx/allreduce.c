#include <string.h>
#include <stdio.h>

#include "hpx.h"
#include <mpi.h>

#define REDUCTION_TYPE int
REDUCTION_TYPE local_reduction_result;
REDUCTION_TYPE* local_reduction_values;
hpx_future_t* local_reduction_futures;
hpx_future_t done_fut;
hpx_action_t* a_done;

void _done_action(void* _args) {
  hpx_lco_future_set_state(&done_fut);
}

struct reduction_recv_args {
  int index;
  REDUCTION_TYPE value;
};

struct reduction_op_args {
  int count;
  REDUCTION_TYPE *in_values;
  int (*op)(int, int[]);
  hpx_future_t* futs;
};

struct recv_broadcast_args {
  int value;
};

int sum(int count, int values[]) {
  int i;
  int sum;
  sum = 0;

  for (i = 0; i < count; i++) {
    sum += values[i];
  }
  return sum;
}

void _reduction_recv_action(void* _args) {
  hpx_locality_t* my_loc = hpx_get_my_locality();
  int my_rank = my_loc->rank;
  //  printf("At %d in _reduction_recv_action\n", my_rank);

  struct reduction_recv_args* args = (struct reduction_recv_args*)_args;
  //  printf("At %d received value to reduce %d\n", my_rank, args->value);
  local_reduction_values[args->index] = args->value;
  hpx_lco_future_set_state(&local_reduction_futures[args->index]);
  //  hpx_free(_args);
}

void _reduction_op_action(void* _args) {
  hpx_locality_t* my_loc = hpx_get_my_locality();
  int my_rank = my_loc->rank;

  /*
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("On %s (rank %d) in _reduction_op_action\n", hostname, my_rank);
  */

  hpx_error_t success;
  int i;
  REDUCTION_TYPE *value;
  struct reduction_op_args* args = (struct reduction_op_args*)_args;
  hpx_parcel_t* p;
  hpx_locality_t* loc;

  //  printf("  _reduction_op_action waiting on %d futures\n", args->count);

  value = hpx_alloc(sizeof(REDUCTION_TYPE));

  for (i = 0; i < args->count; i++) {
    hpx_thread_wait(&args->futs[i]);
    //    printf("  _reduction_op_action done waiting on future %d\n", i);
  }
  
  *value = args->op(args->count, args->in_values);

  for (i = 0; i < args->count; i++) {
    p = hpx_alloc(sizeof(hpx_parcel_t));
    loc = hpx_locality_from_rank(i);
    success = hpx_new_parcel("recv_broadcast_action", (void*)value, sizeof(REDUCTION_TYPE), p);
    if (success != HPX_SUCCESS) {
      printf("Error creating parcel");
      exit(-1);
    }
    success = hpx_send_parcel(loc, p);
    if (success != HPX_SUCCESS) {
      printf("Error sending parcel\n");
      exit(-1);
    }
    hpx_locality_destroy(loc);
  }
  //  hpx_free(_args);
}

void _broadcast_recv_action(void* _args) {
  REDUCTION_TYPE* value = (REDUCTION_TYPE*)_args;
  hpx_locality_t* my_loc = hpx_get_my_locality();
  int my_rank = my_loc->rank;
  //  printf("At %d received reduction value %lld\n", my_rank, (long long)*value);
  local_reduction_result = *value;

  hpx_action_invoke(a_done, NULL, NULL);
}

void allreduce(void *_value) {
  REDUCTION_TYPE value = *(REDUCTION_TYPE*)_value;
  int success;
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_locality_t* root_loc;
  REDUCTION_TYPE local_reduction_value;
  int local_reduction_count;
  hpx_action_t* a_reduce_recv;
  hpx_action_t* a_reduce_op;
  hpx_action_t* a_broadcast_recv;
  struct reduction_op_args* op_args;
  struct reduction_recv_args* args;

  num_ranks = hpx_get_num_localities();
  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;
  root_loc = hpx_locality_from_rank(0);


  
  /* register action for parcel (must be done by all ranks) */
  a_reduce_recv = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("reduction_recv_action", (hpx_func_t)_reduction_recv_action, a_reduce_recv);
  a_reduce_op = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("reduction_op_action", (hpx_func_t)_reduction_op_action, a_reduce_op);
  a_broadcast_recv = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("recv_broadcast_action", (hpx_func_t)_broadcast_recv_action, a_broadcast_recv);
  a_done = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("done_action", (hpx_func_t)_done_action, a_done);

  hpx_lco_future_init(&done_fut);
  if (my_rank == 0) {
    local_reduction_values = hpx_alloc(num_ranks * sizeof(REDUCTION_TYPE));
    local_reduction_futures = hpx_alloc(num_ranks * sizeof(hpx_future_t));
    int i;
    for (i = 0; i < num_ranks; i++) {
      hpx_lco_future_init(&local_reduction_futures[i]);
    }
    local_reduction_value = 0;
    local_reduction_count = num_ranks;
  }

  MPI_Barrier(MPI_COMM_WORLD); // TODO: replace with something more appropriate

  if (my_rank == 0) {
    op_args = hpx_alloc(sizeof(struct reduction_op_args));
    op_args->count     = num_ranks;
    op_args->in_values = local_reduction_values;
    op_args->futs      = local_reduction_futures;
    op_args->op        = sum;

    hpx_action_invoke(a_reduce_op, op_args, NULL);   

    args = hpx_alloc(sizeof(struct reduction_recv_args));
    args->index = 0;
    args->value = value;

    hpx_action_invoke(a_reduce_recv, args, NULL);   
  }
  else {
    args = hpx_alloc(sizeof(struct reduction_recv_args));
    hpx_parcel_t* p;
    hpx_locality_t* loc;

    args->index = my_rank;
    args->value = value;

    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel("reduction_recv_action", (void*)args, sizeof(struct reduction_recv_args), p);
    if (success != HPX_SUCCESS) {
      printf("Error creating parcel");
      exit(-1);
    }
    success = hpx_send_parcel(root_loc, p);
    if (success != HPX_SUCCESS) {
      printf("Error sending parcel\n");
      exit(-1);
    }
  }

  hpx_thread_wait(&done_fut);

  printf("At rank %d received value %lld\n", my_rank, (long long)local_reduction_result);

  hpx_free(a_reduce_recv);
  hpx_free(a_reduce_op);
  hpx_free(a_broadcast_recv);
  hpx_free(a_done);
  hpx_locality_destroy(root_loc);

  // TODO: get a return value instead of printing
}

int main(int argc, char** argv) {
  int success;

  struct timespec begin_ts;
  struct timespec end_ts;
  unsigned long elapsed;
  double avg_oneway_latency;

  success = hpx_init();
  if (success != 0) {
    printf("Error in hpx_init!\n");
    exit(-1);
  }

  unsigned int my_rank;
  hpx_locality_t* my_loc;
  hpx_thread_t* th;
  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);

  hpx_thread_create(__hpx_global_ctx, 0, (hpx_func_t)allreduce, (void*)&my_rank, &th);
  hpx_thread_join(th, NULL);

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);
  
  hpx_cleanup();

  return 0;
}

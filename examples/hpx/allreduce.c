#include <string.h>
#include <stdio.h>

#include "hpx.h"
#include <mpi.h>

#define REDUCTION_TYPE int

static REDUCTION_TYPE local_reduction_result;
static REDUCTION_TYPE* local_reduction_values;
static hpx_future_t* local_reduction_futures;
static hpx_future_t done_fut;

static hpx_action_t done_action = HPX_ACTION_NULL;
static hpx_action_t op_action = HPX_ACTION_NULL;
static hpx_action_t recv_action = HPX_ACTION_NULL;
static hpx_action_t recv_broadcast_action = HPX_ACTION_NULL;

void
done(void* args)
{
  hpx_lco_future_set_state(&done_fut);
}

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


struct reduction_recv_args {
  int index;
  REDUCTION_TYPE value;
};

void reduce_recv(void* _args) {
  /* hpx_locality_t* my_loc = hpx_get_my_locality(); */
  /* int my_rank = my_loc->rank; */
  /* // printf("At %d in _reduction_recv_action\n", my_rank); */

  struct reduction_recv_args* args = (struct reduction_recv_args*)_args;
  //  printf("At %d received value to reduce %d\n", my_rank, args->value);
  local_reduction_values[args->index] = args->value;
  hpx_lco_future_set_state(&local_reduction_futures[args->index]);
  //  hpx_free(_args);
}

void op(void* _args) {
  /* hpx_locality_t* my_loc = hpx_get_my_locality(); */
  /* int my_rank = my_loc->rank; */

  /*
  char hostname[256];
  gethostname(hostname, sizeof(hostname));
  printf("On %s (rank %d) in _reduction_op_action\n", hostname, my_rank);
  */

  hpx_error_t success;
  int i;
  REDUCTION_TYPE *value;
  struct reduction_op_args* args = (struct reduction_op_args*)_args;

  //  printf("  _reduction_op_action waiting on %d futures\n", args->count);

  value = hpx_alloc(sizeof(REDUCTION_TYPE));

  for (i = 0; i < args->count; i++) {
    hpx_thread_wait(&args->futs[i]);
    //    printf("  _reduction_op_action done waiting on future %d\n", i);
  }
  
  *value = args->op(args->count, args->in_values);

  for (i = 0; i < args->count; i++) {
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(REDUCTION_TYPE));
    if (!p) {
      printf("Error creating parcel");
      exit(-1);
    }

    hpx_parcel_set_action(p, recv_broadcast_action);
    void *data = hpx_parcel_get_data(p);
    memcpy(data, &value, sizeof(REDUCTION_TYPE));
    success = hpx_parcel_send(hpx_locality_from_rank(i), p, NULL, NULL, NULL);
    if (success != HPX_SUCCESS) {
      printf("Error sending parcel\n");
      exit(-1);
    }
    hpx_parcel_release(p);
  }
}

void recv_broadcast(void* _args) {
  REDUCTION_TYPE* value = (REDUCTION_TYPE*)_args;
  /* hpx_locality_t* my_loc = hpx_get_my_locality(); */
  /* int my_rank = my_loc->rank; */
  //  printf("At %d received reduction value %lld\n", my_rank, (long long)*value);
  local_reduction_result = *value;

  hpx_action_invoke(done_action, NULL, NULL);
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
  struct reduction_op_args* op_args;
  struct reduction_recv_args* args;

  num_ranks = hpx_get_num_localities();
  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;
  root_loc = hpx_locality_from_rank(0);

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

    hpx_action_invoke(op_action, op_args, NULL);   

    args = hpx_alloc(sizeof(struct reduction_recv_args));
    args->index = 0;
    args->value = value;

    hpx_action_invoke(recv_action, args, NULL);   
  }
  else {
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(*args));
    if (!p) {
      fprintf(stderr, "Error creating parcel\n");
      exit(-1);
    }
    
    hpx_parcel_set_action(p, recv_action);
    args = (struct reduction_recv_args*)hpx_parcel_get_data(p);
    args->index = my_rank;
    args->value = value;
    success = hpx_parcel_send(root_loc, p, NULL, NULL, NULL);
    if (success != HPX_SUCCESS) {
      fprintf(stderr, "Error sending parcel\n");
      exit(-1);
    }
    hpx_parcel_release(p);
  }

  hpx_thread_wait(&done_fut);

  printf("At rank %d received value %lld\n", my_rank, (long long)local_reduction_result);
  hpx_locality_destroy(root_loc);

  // TODO: get a return value instead of printing
}

int
main(int argc, char** argv)
{
  hpx_timer_t timer;
  
  int success = hpx_init();
  if (success != 0) {
    printf("Error %d in hpx_init!\n", success);
    exit(-1);
  }

  /* register action for parcel (must be done by all ranks) */
  recv_action = hpx_action_register("reduction_recv", (hpx_func_t)reduce_recv);
  op_action = hpx_action_register("reduction_op",     (hpx_func_t)op);
  recv_broadcast_action = hpx_action_register("recv_broadcast", (hpx_func_t)recv_broadcast);
  done_action = hpx_action_register("done",           (hpx_func_t)done);
  hpx_action_registration_complete();
  
  hpx_locality_t* my_loc = hpx_get_my_locality();
  unsigned int my_rank = my_loc->rank;

  hpx_get_time(&timer);

  hpx_future_t *complete;
  hpx_thread_create(__hpx_global_ctx, 0, (hpx_func_t)allreduce, (void*)&my_rank,
                    &complete, NULL);
  hpx_thread_wait(complete);

  /* long elapsed = hpx_elapsed_us(timer); */
  
  hpx_cleanup();

  return 0;
}

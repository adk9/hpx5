#include "libapex/apex_global_hpx.h"
#include <math.h>
#include <string.h>

//#define APEX_LOCALITY ((HPX_LOCALITIES)-1) // doesn't seem to work...
#define APEX_LOCALITY 0
apex_profile value;
apex_profile reduced_value;
int min_rank = 0;
int max_rank = 0;

hpx_action_t apex_get_value = 0;
hpx_action_t apex_allreduce = 0;
apex_function_address profiled_action = 0L;
char profiled_action_name[256];
apex_profiler_type profiler_type;

FILE *graph_output;

void apex_sum(int count, apex_profile values[count]) {
  reduced_value.calls = values[0].calls; 
  reduced_value.accumulated = values[0].accumulated; 
  reduced_value.sum_squares = values[0].sum_squares; 
  reduced_value.minimum = values[0].minimum; 
  reduced_value.maximum = values[0].maximum; 
  min_rank = max_rank = 0;
  for (int i = 1; i < count; ++i) { 
	  reduced_value.calls += values[i].calls; 
	  reduced_value.accumulated += values[i].accumulated; 
	  reduced_value.sum_squares += values[i].sum_squares; 
	  //reduced_value.minimum = fmin(reduced_value.minimum,values[i].minimum); 
      if (values[i].minimum < reduced_value.minimum) {
	    reduced_value.minimum = values[i].minimum; 
        min_rank = i;
      }
	  //reduced_value.maximum = fmax(reduced_value.maximum,values[i].maximum); 
	  if (values[i].maximum > reduced_value.maximum) {
	    reduced_value.maximum = values[i].maximum; 
        max_rank = i;
      }
  }
  return ;
}

int apex_get_value_action(void *args) {
  apex_profile * p;
  if (profiler_type == APEX_FUNCTION_ADDRESS) {
    p = apex_get_profile(APEX_FUNCTION_ADDRESS, (void*)profiled_action);
  } else { 
    p = apex_get_profile(APEX_NAME_STRING, (void*)profiled_action_name);
  }
  if (p != NULL) {
    value.calls = p->calls; 
    value.accumulated = p->accumulated; 
    value.sum_squares = p->sum_squares; 
    value.minimum = p->minimum; 
    value.maximum = p->maximum; 
  }
  HPX_THREAD_CONTINUE(value);
  return HPX_SUCCESS;
}

int apex_allreduce_action(void *unused) {
  int num_ranks = HPX_LOCALITIES;
  int my_rank = HPX_LOCALITY_ID;
  // The APEX sink is the only one to make the requests
  if (my_rank != APEX_LOCALITY) {
      return HPX_SUCCESS;
  }

  apex_profile values[num_ranks];
  void         *addrs[num_ranks];
  int          sizes[num_ranks];
  hpx_addr_t   futures[num_ranks];

  for (int i = 0; i < num_ranks; ++i) {
    addrs[i] = &values[i];
    sizes[i] = sizeof(apex_profile);
    futures[i] = hpx_lco_future_new(sizeof(apex_profile));
    hpx_call(HPX_THERE(i), apex_get_value, futures[i], NULL, 0);
  }
  hpx_lco_get_all(num_ranks, futures, sizes, addrs, NULL);
  apex_sum(num_ranks, values);
  for (int i = 0; i < num_ranks; ++i) {
    hpx_lco_delete(futures[i], HPX_NULL);
  }
  return HPX_SUCCESS;
}

void apex_trigger_reduction(void) {
  int my_rank = HPX_LOCALITY_ID;
  if (my_rank == APEX_LOCALITY) {
    hpx_addr_t future = hpx_lco_future_new(sizeof(int));
    // fire and forget...
    hpx_call(HPX_HERE, apex_allreduce, future, NULL, 0);
    //hpx_call(APEX_LOCALITY, apex_allreduce, future, NULL, 0);
    //apex_allreduce_action(NULL);
  }
  return;
}

int apex_periodic_output(apex_context const context) {
  int my_rank = HPX_LOCALITY_ID;
  if (my_rank != APEX_LOCALITY) return APEX_NOERROR;
  double avg = 0.0;
  double stddev = 0.0;
  if (reduced_value.calls > 0.0) {
    avg = reduced_value.accumulated / reduced_value.calls;
    stddev = sqrt((reduced_value.sum_squares / reduced_value.calls) - (avg*avg));
  }
  printf("Iterations: calls=%lu mean=%f +/- %f, min=%f (%d), max=%f (%d)\r", (unsigned long)reduced_value.calls, avg, stddev, reduced_value.minimum, min_rank, reduced_value.maximum, max_rank);
  fflush(stdout);
  fprintf(graph_output,"%f %f\n",avg, stddev);
  fflush(graph_output);
  return APEX_NOERROR;
}

void apex_global_setup(apex_profiler_type type, void* in_action) {
  profiler_type = type;
  if (type == APEX_FUNCTION_ADDRESS) {
    profiled_action = (apex_function_address)in_action;
  } else {
    strcpy(profiled_action_name, in_action);
  }
  apex_register_periodic_policy(1000000, apex_periodic_output);
  apex_set_use_policy(true);
  //set_value      = HPX_REGISTER_ACTION(action_apex_set_value);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, 
          apex_get_value, apex_get_value_action, 
          HPX_POINTER, HPX_SIZE_T);
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, 
          apex_allreduce, apex_allreduce_action, 
          HPX_POINTER, HPX_SIZE_T );
  graph_output = fopen("./profile_data.txt", "w");
}


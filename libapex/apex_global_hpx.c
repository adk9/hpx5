#include "libapex/apex_global_hpx.h"
#include <math.h>

#define APEX_LOCALITY ((HPX_LOCALITIES)-1)
apex_profile value;
apex_profile reduced_value;

hpx_action_t apex_get_value = 0;
hpx_action_t apex_allreduce = 0;
apex_function_address profiled_action = 0L;

FILE *graph_output;

void apex_sum(int count, apex_profile values[count]) {
  for (int i = 0; i < count; ++i) { 
	  reduced_value.calls += values[i].calls; 
	  reduced_value.accumulated += values[i].accumulated; 
	  reduced_value.sum_squares += values[i].sum_squares; 
	  reduced_value.minimum = fmin(reduced_value.minimum,values[i].minimum); 
	  reduced_value.maximum = fmax(reduced_value.maximum,values[i].maximum); 
  }
  return ;
}

int apex_get_value_action(void *args) {
  apex_profile * p = apex_get_profile(APEX_FUNCTION_ADDRESS, (void*)profiled_action);
  if (p != NULL) {
    value.calls = p->calls; 
    value.accumulated = p->accumulated; 
    value.sum_squares = p->sum_squares; 
    value.minimum = p->minimum; 
    value.maximum = p->maximum; 
  }
  HPX_THREAD_CONTINUE(value);
}

void apex_trigger_reduction(void) {
    /*
  hpx_addr_t future = hpx_lco_future_new(sizeof(int));
  int dummy = 0;
  hpx_call(HPX_HERE, apex_allreduce, future, &dummy, sizeof(int));
  apex_allreduce_action(NULL);
  */
}

int apex_allreduce_action(void *unused) {
  int num_ranks = HPX_LOCALITIES;
  int my_rank = HPX_LOCALITY_ID;
  // The APEX sink is the last locality
  /*
  if (my_rank != APEX_LOCALITY) {
      printf("%d not me!\n", my_rank);
      return HPX_SUCCESS;
  }
  */

  apex_profile values[num_ranks];
  void         *addrs[num_ranks];
  int          sizes[num_ranks];
  hpx_addr_t   futures[num_ranks];
  //hpx_addr_t   result;

  for (int i = 0; i < num_ranks; ++i) {
    addrs[i] = &values[i];
    sizes[i] = sizeof(apex_profile);
    futures[i] = hpx_lco_future_new(sizeof(apex_profile));
    hpx_call(HPX_THERE(i), apex_get_value, (hpx_addr_t)(NULL), 0, futures[i]);
  }
  hpx_lco_get_all(num_ranks, futures, sizes, addrs, NULL);
  apex_sum(num_ranks, values);
  return HPX_SUCCESS;
}

int apex_periodic_output(apex_context const context) {
  int my_rank = HPX_LOCALITY_ID;
  if (my_rank != APEX_LOCALITY) return APEX_NOERROR;
  double avg = 0.0;
  double stddev = 0.0;
  if (value.calls > 0.0) {
    avg = value.accumulated / value.calls;
    stddev = sqrt((value.sum_squares / value.calls) - (avg*avg));
  }
  printf("global action: calls=%lu mean=%f +/- %f\r", (unsigned long)value.calls, avg, stddev);
  fflush(stdout);
  fprintf(graph_output,"%f %f\n",avg, stddev);
  fflush(graph_output);
  return APEX_NOERROR;
}

void apex_global_setup(apex_function_address in_action) {
  profiled_action = in_action;
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


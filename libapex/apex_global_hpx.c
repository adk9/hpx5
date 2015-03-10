#include "apex_global_hpx.h"

apex_profile value;
apex_profile reduced_value;

hpx_action_t get_value = 0;
hpx_action_t action_apex_allreduce_handle = 0;
apex_function_address profiled_action = 0L;

FILE *graph_output;

#define min(x,y) ((x) < (y) ? (x) : (y))
#define max(x,y) ((x) > (y) ? (x) : (y))

void apex_sum(int count, apex_profile values[count]) {
  for (int i = 0; i < count; ++i) { 
	  reduced_value.calls += values[i].calls; 
	  reduced_value.accumulated += values[i].accumulated; 
	  reduced_value.sum_squares += values[i].sum_squares; 
	  reduced_value.minimum = min(reduced_value.minimum,values[i].minimum); 
	  reduced_value.maximum = max(reduced_value.maximum,values[i].maximum); 
  }
  return ;
}

int action_apex_get_value(void *args) {
  apex_profile * p = apex_get_profile_from_address(profiled_action);
  if (p != NULL) {
    value.calls = p->calls; 
    value.accumulated = p->accumulated; 
    value.sum_squares = p->sum_squares; 
    value.minimum = p->minimum; 
    value.maximum = p->maximum; 
  }
  HPX_THREAD_CONTINUE(value);
}

int action_apex_allreduce(void *unused) {
  int num_ranks = HPX_LOCALITIES;
  int my_rank = HPX_LOCALITY_ID;
  if (my_rank != 0) return HPX_SUCCESS;

  apex_profile values[num_ranks];
  void         *addrs[num_ranks];
  int          sizes[num_ranks];
  hpx_addr_t   futures[num_ranks];
  //hpx_addr_t   result;

  for (int i = 0; i < num_ranks; ++i) {
    addrs[i] = &values[i];
    sizes[i] = sizeof(apex_profile);
    futures[i] = hpx_lco_future_new(sizeof(apex_profile));
    hpx_call(HPX_THERE(i), get_value, (hpx_addr_t)(NULL), 0, futures[i]);
  }
  hpx_lco_get_all(num_ranks, futures, sizes, addrs, NULL);
  apex_sum(num_ranks, values);
  return HPX_SUCCESS;
}

int apex_periodic_output(apex_context const context) {
  //int dummy = 0;
  //hpx_call(HPX_HERE, action_apex_allreduce_handle, &dummy, sizeof(int), HPX_NULL);
  int my_rank = HPX_LOCALITY_ID;
  if (my_rank != 0) return 1;
  double avg = 0.0;
  double stddev = 0.0;
  if (value.calls > 0.0) {
    avg = value.accumulated / value.calls;
    stddev = sqrt((value.sum_squares / value.calls) - (avg*avg));
  }
  printf("_advanceDomain_action: calls=%lu mean=%f +/- %f\r", (unsigned long)value.calls, avg, stddev);
  fflush(stdout);
  fprintf(graph_output,"%f %f\n",avg, stddev);
  fflush(graph_output);
  return 1;
}

void apex_global_setup(apex_function_address in_action) {
  profiled_action = in_action;
  apex_register_periodic_policy(1000000, apex_periodic_output);
  apex_set_use_policy(true);
  //set_value      = HPX_REGISTER_ACTION(action_apex_set_value);
  HPX_REGISTER_ACTION(action_apex_get_value, &get_value);
  HPX_REGISTER_ACTION(action_apex_allreduce, &action_apex_allreduce_handle );
  graph_output = fopen("./profile_data.txt", "w");
}


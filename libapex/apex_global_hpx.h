#ifndef APEX_GLOBAL_APEX_H
#define APEX_GLOBAL_APEX_H

#include "apex.h"
#include "math.h"
#include "stdio.h"
#include "hpx/hpx.h"
#include "apex.h"

extern apex_profile value;
extern apex_profile reduced_value;

extern hpx_action_t set_value;
extern hpx_action_t get_value;
extern hpx_action_t action_apex_allreduce_handle;

//void apex_sum(int count, apex_profile values[count]) ;
//int action_apex_get_value(void *args) ;
//int action_apex_set_value(void *args) ;
//int action_apex_allreduce(void *unused) ;
//int apex_periodic_policy_func(apex_context const context) ;
//void apex_global_setup(apex_function_address in_action);

#endif

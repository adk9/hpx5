/// ----------------------------------------------------------------------------
/// @file fmm-action.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief Declare FMM actions 
/// ----------------------------------------------------------------------------

#include "hpx/hpx.h"

extern hpx_action_t _fmm_main; 
extern hpx_action_t _init_param; 
extern hpx_action_t _partition_box; 

/// ----------------------------------------------------------------------------
/// @brief The main FMM action
/// ----------------------------------------------------------------------------
int _fmm_main_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Initialize FMM param action
/// ----------------------------------------------------------------------------
int _init_param_action(void *args); 

/// ----------------------------------------------------------------------------
/// @brief Construct the FMM DAG
/// ----------------------------------------------------------------------------
int _partition_box_action(void *args); 

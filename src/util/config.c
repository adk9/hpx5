/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Configuration Functions
  hpx_config.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
    Patrick K. Bohan <pbohan [at] indiana.edu>
 ====================================================================
*/

#include "hpx/config.h"
#include "hpx/kthread.h"

/*
 --------------------------------------------------------------------
  hpx_config_init

  Initializes configuration data for HPX.
 --------------------------------------------------------------------
*/
void hpx_config_init(hpx_config_t *cfg) {
  hpx_config_set_cores(cfg, hpx_kthread_get_cores());
  hpx_config_set_switch_flags(cfg, HPX_CONFIG_DEFAULT_SWITCH_FLAG);
  hpx_config_set_thread_stack_size(cfg, HPX_CONFIG_DEFAULT_THREAD_SS);
}


/*
 --------------------------------------------------------------------
  hpx_config_get_cores

  Gets the number of configured CPU cores.
 --------------------------------------------------------------------
*/
uint32_t hpx_config_get_cores(hpx_config_t *cfg) {
  return cfg->cores;
}


/*
 --------------------------------------------------------------------
  hpx_config_get_switch_flags

  Returns machine context switching flags.
 --------------------------------------------------------------------
*/
uint64_t hpx_config_get_switch_flags(hpx_config_t *cfg) {
  return cfg->mflags;
}


/*
 --------------------------------------------------------------------
  hpx_config_get_thread_stack_size

  Returns the number of bytes allocated for thread stacks.
 --------------------------------------------------------------------
*/
uint32_t hpx_config_get_thread_stack_size(hpx_config_t *cfg) {
  return cfg->thread_ss;
}


/*
 --------------------------------------------------------------------
  hpx_config_set_cores

  Sets the number of configured CPU cores.
 --------------------------------------------------------------------
*/
void hpx_config_set_cores(hpx_config_t *cfg, uint32_t cores) {
  if (cores > 0) {
    cfg->cores = cores;
  }
}


/*
 --------------------------------------------------------------------
  hpx_config_set_switch_flags

  Sets machine context switching flags.
 --------------------------------------------------------------------
*/
void hpx_config_set_switch_flags(hpx_config_t *cfg, uint64_t flags) {
  cfg->mflags = flags;
}


/*
 --------------------------------------------------------------------
  hpx_config_set_thread_stack_size

  Sets the number of bytes allocated for thread stacks.
 --------------------------------------------------------------------
*/
void hpx_config_set_thread_stack_size(hpx_config_t *cfg, uint32_t ss) {
  cfg->thread_ss = ss;
}

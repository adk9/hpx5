/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Configuration Function Definitions
  hpx_config.h

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

#pragma once
#ifndef HPX_CONFIG_H_
#define HPX_CONFIG_H_

#include <stdint.h>

typedef struct hpx_config hpx_config_t;

/*
  --------------------------------------------------------------------
  Thread Suspension Policies
 --------------------------------------------------------------------
*/
#define HPX_CONFIG_THREAD_SUSPEND_SRV_LOCAL   0
#define HPX_CONFIG_THREAD_SUSPEND_SRV_GLOBAL  1


/*
 --------------------------------------------------------------------
  Defaults
 --------------------------------------------------------------------
*/
#define HPX_CONFIG_DEFAULT_THREAD_SS              16384
#define HPX_CONFIG_DEFAULT_SWITCH_FLAG            0
#define HPX_CONFIG_DEFAULT_THREAD_SUSPEND_POLICY  1

/*
 --------------------------------------------------------------------
  General Configuration Data
 --------------------------------------------------------------------
*/

struct hpx_config {
  uint32_t cores;
  uint64_t mflags;
  uint32_t thread_ss;
  uint8_t  srv_susp_policy;
};


/*
 --------------------------------------------------------------------
  General Configuration Functions
 --------------------------------------------------------------------
*/
void hpx_config_init(hpx_config_t *);

uint32_t hpx_config_get_cores(hpx_config_t *);
uint64_t hpx_config_get_switch_flags(hpx_config_t *);
uint32_t hpx_config_get_thread_stack_size(hpx_config_t *);
uint8_t hpx_config_get_thread_suspend_policy(hpx_config_t *);

void hpx_config_set_cores(hpx_config_t *, uint32_t);
void hpx_config_set_switch_flags(hpx_config_t *, uint64_t);
void hpx_config_set_thread_stack_size(hpx_config_t *, uint32_t);
void hpx_config_set_thread_suspend_policy(hpx_config_t *, uint8_t);

#endif /* HPX_CONFIG_H_ */

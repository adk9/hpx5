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
#ifndef LIBHPX_CONFIG_H_
#define LIBHPX_CONFIG_H_

#include <stdint.h>

/*
 --------------------------------------------------------------------
  Defaults
 --------------------------------------------------------------------
*/

#define HPX_CONFIG_DEFAULT_THREAD_SS     16384
#define HPX_CONFIG_DEFAULT_SWITCH_FLAG   0


/*
 --------------------------------------------------------------------
  General Configuration Data
 --------------------------------------------------------------------
*/

typedef struct {
  uint32_t cores;
  uint64_t mflags;
  uint32_t thread_ss;
} hpx_config_t;


/*
 --------------------------------------------------------------------
  General Configuration Functions
 --------------------------------------------------------------------
*/

void hpx_config_init(hpx_config_t *);

uint32_t hpx_config_get_cores(hpx_config_t *);
uint64_t hpx_config_get_switch_flags(hpx_config_t *);
uint32_t hpx_config_get_thread_stack_size(hpx_config_t *);

void hpx_config_set_cores(hpx_config_t *, uint32_t);
void hpx_config_set_switch_flags(hpx_config_t *, uint64_t);
void hpx_config_set_thread_stack_size(hpx_config_t *, uint32_t);

#endif /* LIBHPX_CONFIG_H_ */

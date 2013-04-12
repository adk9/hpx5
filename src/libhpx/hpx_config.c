
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

#include "hpx_config.h"
#include "hpx_kthread.h"


/*
 --------------------------------------------------------------------
  hpx_config_init

  Initializes configuration data for HPX-4.
 --------------------------------------------------------------------
*/

void hpx_config_init(hpx_config_t * cfg) {
  hpx_config_set_cores(cfg, hpx_kthread_get_cores());
}


/*
 --------------------------------------------------------------------
  hpx_config_get_cores

  Gets the number of configured CPU cores.
 --------------------------------------------------------------------
*/

uint32_t hpx_config_get_cores(hpx_config_t * cfg) {
  return cfg->cores;
}


/*
 --------------------------------------------------------------------
  hpx_config_set_cores

  Sets the number of configured CPU cores.
 --------------------------------------------------------------------
*/

void hpx_config_set_cores(hpx_config_t * cfg, uint32_t cores) {
  cfg->cores = cores;
}

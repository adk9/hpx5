
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


/*
 --------------------------------------------------------------------
  General Configuration Data
 --------------------------------------------------------------------
*/

typedef struct {
  uint32_t cores;
} hpx_config_t;


/*
 --------------------------------------------------------------------
  General Configuration Functions
 --------------------------------------------------------------------
*/

hpx_config_init(hpx_config_t *);

hpx_config_get_cores(hpx_config_t *);

hpx_config_set_cores(hpx_config_t *);

#endif



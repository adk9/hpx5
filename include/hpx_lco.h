
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Local Control Object (LCO) Function Definitions
  hpx_lco.h

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

#include <stdint.h>

#pragma once
#ifndef LIBHPX_LCO_H_
#define LIBHPX_LCO_H_

#define HPX_LCO_FUTURE_UNSET                                        0
#define HPX_LCO_FUTURE_SET                                          1


/*
 --------------------------------------------------------------------
  LCO Data
 --------------------------------------------------------------------
*/

typedef struct _hpx_future_t {
  uint8_t state;
  void * value;
} hpx_future_t;


/*
 --------------------------------------------------------------------
  LCO Functions
 --------------------------------------------------------------------
*/

void hpx_lco_future_init(hpx_future_t *);
void hpx_lco_future_set(hpx_future_t *);

#endif



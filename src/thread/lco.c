/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Library initialization and cleanup functions
  hpx_init.c

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

#include <stdlib.h>

#include "hpx/lco.h"
#include "hpx/atomic.h"

/*
 --------------------------------------------------------------------
  hpx_lco_future_init

  Initializes an HPX Future.
 --------------------------------------------------------------------
*/

//void hpx_lco_future_init(hpx_future_t *fut) {
//  fut->value = NULL;
//  fut->state = HPX_LCO_FUTURE_UNSET;
//}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set

  Sets the state of an HPX Future to SET.
 --------------------------------------------------------------------
*/

//void hpx_lco_future_set(hpx_future_t *fut) {
//  hpx_atomic_set8(&fut->state, HPX_LCO_FUTURE_SET);
//}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value

  Sets the value of an HPX Future and sets its state to SET.
 --------------------------------------------------------------------
*/

//void hpx_lco_future_set_value(hpx_future_t *fut, void *val) {
//  fut->value = val;
//  hpx_atomic_set8(&fut->state, HPX_LCO_FUTURE_SET);
//}


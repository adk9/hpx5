
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
} hpx_future_t __attribute__((aligned (8)));


/*
 --------------------------------------------------------------------
  LCO Functions
 --------------------------------------------------------------------
*/

/*
 --------------------------------------------------------------------
  hpx_lco_future_init

  Sets the value of an HPX Future to NULL and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

inline void hpx_lco_future_init(hpx_future_t * fut) {
  fut->state = HPX_LCO_FUTURE_UNSET;
  fut->value = NULL;
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_lco_future_set

  Sets an HPX Future's state to SET without changing its value.
 --------------------------------------------------------------------
*/

inline void hpx_lco_future_set(hpx_future_t * fut) {
#ifdef __x86_64__
  __asm__ __volatile__ (
    "movb %1,%%al;\n\t"
    "lock; xchgb %%al,%0;\n\t"
    :"=m" (fut->state)
    :"i" (HPX_LCO_FUTURE_SET)
    :"%al");
#endif
} __attribute__((always_inline));


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value

  Sets an HPX Future's value and its state to SET.
 --------------------------------------------------------------------
*/

inline void hpx_lco_future_set_value(hpx_future_t * fut, void * val) {
  fut->value = val;
  hpx_lco_future_set(fut);
} __attribute__((always_inline));

#endif



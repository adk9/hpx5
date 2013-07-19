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

#pragma once
#ifndef LIBHPX_LCO_H_
#define LIBHPX_LCO_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "hpx/error.h"

#define HPX_LCO_FUTURE_UNSET  0x00
#define HPX_LCO_FUTURE_SET    0xFF


/*
 --------------------------------------------------------------------
  LCO Data
 --------------------------------------------------------------------
*/
typedef struct _hpx_future_t {
  uint8_t *states;
  void   **values;
  uint64_t count;
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
static inline void hpx_lco_future_init(hpx_future_t *fut, uint64_t num) {
  /* initialize */
  fut->states = NULL;
  fut->values = NULL;
  fut->count = 0;

  /* allocate states */
  fut->states = (uint8_t*) hpx_alloc(num);
  if (fut->states == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
  } else {
    /* allocate values */
    fut->values = (void**) hpx_alloc(num * sizeof(void*));
    if (fut->values == NULL) {
      __hpx_errno = HPX_ERROR_NOMEM;
    } else {
      memset(fut->states, HPX_LCO_FUTURE_UNSET, num * sizeof(uint8_t));
      memset(fut->values, 0, num * sizeof(void*));
      fut->count = num;
    }
  }
}


/*
  --------------------------------------------------------------------
  hpx_lco_future_destroy

  Destroys a future.
 --------------------------------------------------------------------
*/
static inline void hpx_lco_future_destroy(hpx_future_t *fut) {
  hpx_free(fut->states);
  hpx_free(fut->values);

  fut->states = NULL;
  fut->values = NULL;
  fut->count = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set

  Sets an HPX Future's state to SET without changing its value.
 --------------------------------------------------------------------
*/
static inline void hpx_lco_future_set(hpx_future_t *fut, uint64_t idx) {
#ifdef __x86_64__
  __asm__ __volatile__ (
    "movb %1,%%al;\n\t"
    "lock; xchgb %%al,%0;\n\t"
    :"=m" (fut->states[idx])
    :"i" (HPX_LCO_FUTURE_SET)
    :"%al");
#endif
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value

  Sets an HPX Future's value and its state to SET.
 --------------------------------------------------------------------
*/
static inline void hpx_lco_future_set_value(hpx_future_t *fut, uint64_t idx, void *val) {
  fut->values[idx] = val;
  hpx_lco_future_set(fut, idx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_isset

  Determines whether or not an HPX Future is in the SET state.
 --------------------------------------------------------------------
*/

static inline bool hpx_lco_future_isset(hpx_future_t *fut) {
  bool isset = true;
  uint64_t idx;
 
  for (idx = 0; idx < fut->count; idx++) {
    if (fut->states[idx] != HPX_LCO_FUTURE_SET) {
      isset = false;
      break;
    }
  }

  return isset;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline void * hpx_lco_future_get_value(hpx_future_t *fut, uint64_t idx) {
  return fut->values[idx];
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_state

  Gets the state of a future.
 --------------------------------------------------------------------
*/

static inline uint8_t hpx_lco_future_get_state(hpx_future_t *fut, uint64_t idx) {
  return fut->states[idx];
}

#endif /* LIBHPX_LCO_H_ */

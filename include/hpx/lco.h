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
#include "hpx/mutex.h"

#define HPX_LCO_FUTURE_SETMASK    0x8000000000000000


/*
 --------------------------------------------------------------------
  LCO Data
 --------------------------------------------------------------------
*/

typedef void * (*hpx_lco_future_pred_t)(void *, void *);

typedef struct _hpx_future_t {
  hpx_mutex_t mtx;
  uint64_t    state;
  void       *value;
} hpx_future_t;


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

static inline void hpx_lco_future_init(hpx_future_t *fut) {
  /* initialize */
  hpx_lco_mutex_init(&fut->mtx, 0);
  fut->state = 0x0000000000000000;
  fut->value = NULL;
}


/*
  --------------------------------------------------------------------
  hpx_lco_future_destroy

  Destroys a future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_destroy(hpx_future_t *fut) {

}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set(hpx_future_t * fut, uint64_t state, void * value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_state

  Sets an HPX Future's state to SET without changing its value.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_state(hpx_future_t *fut) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state |= HPX_LCO_FUTURE_SETMASK;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value(hpx_future_t *fut, void *value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_isset

  Determines whether or not an HPX Future is in the SET state.
 --------------------------------------------------------------------
*/

static inline bool hpx_lco_future_isset(hpx_future_t *fut) {
  if (fut->state & HPX_LCO_FUTURE_SETMASK) {
    return true;
  } else {
    return false;
  }
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline void * hpx_lco_future_get_value(hpx_future_t *fut) {
  return fut->value;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_state

  Gets the state of a future.
 --------------------------------------------------------------------
*/

static inline uint64_t hpx_lco_future_get_state(hpx_future_t *fut) {
  return fut->state;
}


/*
 --------------------------------------------------------------------
  _hpx_lco_future_wait_pred

  Internal predicate function for use with _hpx_thread_wait()
 --------------------------------------------------------------------
*/

static bool _hpx_lco_future_wait_pred(void * target, void * arg) {
  return hpx_lco_future_isset((hpx_future_t *) target);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_apply_pred

  Applies a predicate to the value of a future.  That is, it calls
  the predicate function and atomically replaces the value of the
  future with the return value of the predicate.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_apply_pred(hpx_future_t * fut, hpx_lco_future_pred_t pred, void * userdata) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value = pred(fut->value, userdata);
  hpx_lco_mutex_unlock(&fut->mtx);
}

#endif /* LIBHPX_LCO_H_ */

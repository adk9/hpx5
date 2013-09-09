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

#define HPX_LCO_FUTURE_SETMASK    0x8000000000000000


/*
 --------------------------------------------------------------------
  LCO Data
 --------------------------------------------------------------------
*/

typedef struct _hpx_future_t {
  uint64_t   state;
  void      *value;
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
#ifdef __x86_64__
  __asm__ __volatile__(
    "__HLFS%=:\n\t"
    "movq  %4,%%r10;\n\t"
    "movq  %0,%%rax;\n\t"
    "movq  %1,%%rdx;\n\t"
    "movq  %2,%%rbx;\n\t"
    "movq  %3,%%rcx;\n\t"
    "orq   %%r10,%%rbx;\n\t"
    "lock; cmpxchg16b %0;\n\t"
    "jnz __HLFS%=;\n\t"
    :"=m" (fut->state), "=m" (fut->value)
    :"m" (state), "m" (value), "i" (HPX_LCO_FUTURE_SETMASK)
    :"%rax", "%rbx", "%rcx", "%rdx", "%r10");
#endif
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_state

  Sets an HPX Future's state to SET without changing its value.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_state(hpx_future_t *fut) {
#ifdef __x86_64__
  __asm__ __volatile__(
    "movq %1,%%rax;\n\t"
    "__HLFSS%=:\n\t"
    "movq %2,%%r11;\n\t"
    "orq  %%rax,%%r11;\n\t"
    "lock; cmpxchg %%r11,%0;\n\t"
    "jnz __HLFSS%=;\n\t"
    :"=m" (fut->state)
    :"m" (fut->state), "i" (HPX_LCO_FUTURE_SETMASK)
    :"%rax", "%r11");
#endif
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value(hpx_future_t *fut, void *val) {
#ifdef __x86_64__
  __asm__ __volatile__(
    "movq %0,%%rax;\n\t"
    "__HLFSV%=:\n\t"
    "movq %1,%%r11;\n\t"
    "lock; cmpxchg %%r11,%0;\n\t"
    "jnz __HLFSV%=;\n\t"
    :"=m" (fut->value)
    :"m" (val)
    :"%rax", "%r11");
#endif
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

#endif /* LIBHPX_LCO_H_ */

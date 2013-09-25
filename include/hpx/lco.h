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

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

#include "hpx/error.h"
#include "hpx/mutex.h"

#define HPX_LCO_FUTURE_SETMASK    0x8000000000000000


/*
 --------------------------------------------------------------------
  LCO Data
 --------------------------------------------------------------------
*/

typedef union _hpx_future_value_t_t {
  __m128i         i128;
  int64_t         i64;
  int32_t         i32;
  int16_t         i16;
  int8_t          i8;
  void           *vp;
  float           f32;
  double          f64;
  long double     f80;
  __m128d         f128;
} hpx_future_value_t;

typedef hpx_future_value_t (*hpx_lco_future_pred_t)(void *, void *);

typedef struct _hpx_future_t {
  hpx_mutex_t           mtx;
  uint64_t              state;
  hpx_future_value_t    value;
} hpx_future_t __attribute__((aligned (8)));


/*
 --------------------------------------------------------------------
  LCO Functions
 --------------------------------------------------------------------
*/

/*
 --------------------------------------------------------------------
  __hpx_lco_future_init

  Internal function to initialize the future mutex and state (but
  not the value).
 --------------------------------------------------------------------
*/

static inline void __hpx_lco_future_init(hpx_future_t *fut) {
  hpx_lco_mutex_init(&fut->mtx, 0);
  fut->state = 0x0000000000000000;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init

  (Pointer Future)

  Sets the value of an HPX Future to NULL and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.vp = NULL;
}

/*
 --------------------------------------------------------------------
  hpx_lco_future_init_i8

  (8-bit Integer Future)

  Sets the value of an HPX Future to 0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_i8(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.i8 = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_i16

  (16-bit Integer Future)

  Sets the value of an HPX Future to 0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_i16(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.i16 = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_i32

  (32-bit Integer Future)

  Sets the value of an HPX Future to 0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_i32(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.i32 = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_i64

  (64-bit Integer Future)

  Sets the value of an HPX Future to 0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_i64(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.i64 = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_i128

  (128-bit Integer Future)

  Sets the value of an HPX Future to 0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_i128(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.i128 = _mm_setzero_si128();
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_f32

  (Single Precision Floating Point Future)

  Sets the value of an HPX Future to 0.0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_f32(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.f32 = 0.0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_f64

  (Double Precision Floating Point Future)

  Sets the value of an HPX Future to 0.0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_f64(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.f64 = 0.0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_f80

  (80-bit [Long] Double Precision Floating Point Future)

  Sets the value of an HPX Future to 0.0 and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_f80(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.f80 = 0.0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_init_f128

  (2x Packed Double Precision Floating Point Future)

  Sets the value of an HPX Future to NULL and puts it in the 
  UNSET state.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_init_f128(hpx_future_t *fut) {
  __hpx_lco_future_init(fut);
  fut->value.f128 = _mm_setzero_pd();
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

  (Pointer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set(hpx_future_t * fut, uint64_t state, void * value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.vp = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_i8

  (8-bit Integer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_i8(hpx_future_t * fut, uint64_t state, int8_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.i8 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_i16

  (16-bit Integer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_i16(hpx_future_t * fut, uint64_t state, int16_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.i16 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_i32

  (32-bit Integer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_i32(hpx_future_t * fut, uint64_t state, int32_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.i32 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_i64

  (64-bit Integer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_i64(hpx_future_t * fut, uint64_t state, int64_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.i64 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_i128

  (128-bit Integer Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_i128(hpx_future_t * fut, uint64_t state, __m128i value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.i128 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_f32

  (Single Precision Floating Point Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_f32(hpx_future_t * fut, uint64_t state, float value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.f32 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_f64

  (Double Precision Floating Point Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_f64(hpx_future_t * fut, uint64_t state, double value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.f64 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_f80

  (80-bit [Long] Double Precision Floating Point Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_f80(hpx_future_t * fut, uint64_t state, long double value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.f80 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_f128

  (Packed Double Precision Floating Point Future)

  Sets the state and value of a future at the same time.

  NOTE: This triggers the future.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_f128(hpx_future_t * fut, uint64_t state, __m128d value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->state = (state | HPX_LCO_FUTURE_SETMASK);
  fut->value.f128 = value;
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

  (Pointer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value(hpx_future_t *fut, void *value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.vp = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_i8

  (8-bit Integer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_i8(hpx_future_t *fut, int8_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.i8 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_i16

  (16-bit Integer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_i16(hpx_future_t *fut, int16_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.i16 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_i32

  (32-bit Integer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_i32(hpx_future_t *fut, int32_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.i32 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_i64

  (64-bit Integer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_i64(hpx_future_t *fut, int64_t value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.i64 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_i128

  (128-bit Integer Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_i128(hpx_future_t *fut, __m128i value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.i128 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_f32

  (Single Precision Floating Point Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_f32(hpx_future_t *fut, float value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.f32 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_f64

  (Double Precision Floating Point Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_f64(hpx_future_t *fut, double value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.f64 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_f80

  (80-bit [Long] Double Precision Floating Point Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_f80(hpx_future_t *fut, long double value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.f80 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_set_value_f128

  (Packed Double Precision Floating Point Future)

  Atomically sets an HPX Future's value (but not its state).
 --------------------------------------------------------------------
*/

static inline void hpx_lco_future_set_value_f128(hpx_future_t *fut, __m128d value) {
  hpx_lco_mutex_lock(&fut->mtx);
  fut->value.f128 = value;
  hpx_lco_mutex_unlock(&fut->mtx);
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_isset

  Determines whether or not an HPX Future is in the SET state.
 --------------------------------------------------------------------
*/

static inline bool hpx_lco_future_isset(hpx_future_t *fut) {
  bool retval = false;

  hpx_lco_mutex_lock(&fut->mtx);
  if ((fut->state & HPX_LCO_FUTURE_SETMASK) == HPX_LCO_FUTURE_SETMASK) {
    retval = true;
  }
  hpx_lco_mutex_unlock(&fut->mtx);

  return retval;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value

  (Pointer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline void * hpx_lco_future_get_value(hpx_future_t *fut) {
  void * val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.vp;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_i8

  (8-bit Integer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline int8_t hpx_lco_future_get_value_i8(hpx_future_t *fut) {
  int8_t val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.i8;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_i16

  (16-bit Integer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline int16_t hpx_lco_future_get_value_i16(hpx_future_t *fut) {
  int16_t val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.i16;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_i32

  (32-bit Integer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline int32_t hpx_lco_future_get_value_i32(hpx_future_t *fut) {
  int32_t val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.i32;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_i64

  (64-bit Integer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline int64_t hpx_lco_future_get_value_i64(hpx_future_t *fut) {
  int64_t val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.i64;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_i128

  (128-bit Integer Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline __m128i hpx_lco_future_get_value_i128(hpx_future_t *fut) {
  __m128i val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.i128;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_f32

  (Single Precision Floating Point Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline float hpx_lco_future_get_value_f32(hpx_future_t *fut) {
  float val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.f32;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_f64

  (Double Precision Floating Point Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline double hpx_lco_future_get_value_f64(hpx_future_t *fut) {
  double val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.f64;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_f80

  (80-bit [Long] Double Precision Floating Point Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline long double hpx_lco_future_get_value_f80(hpx_future_t *fut) {
  long double val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.f80;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_value_f128

  (Packed Double Precision Floating Point Future)

  Obtains the value of a future.
 --------------------------------------------------------------------
*/

static inline __m128d hpx_lco_future_get_value_f128(hpx_future_t *fut) {
  __m128d val;

  hpx_lco_mutex_lock(&fut->mtx);
  val = fut->value.f128;
  hpx_lco_mutex_unlock(&fut->mtx);

  return val;
}


/*
 --------------------------------------------------------------------
  hpx_lco_future_get_state

  Gets the state of a future.
 --------------------------------------------------------------------
*/

static inline uint64_t hpx_lco_future_get_state(hpx_future_t *fut) {
  uint64_t state;

  hpx_lco_mutex_lock(&fut->mtx);
  state = fut->state;
  hpx_lco_mutex_unlock(&fut->mtx);

  return state;
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

/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Gate Control Definitions
  gate.h

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
#ifndef LIBHPX_GATE_H_
#define LIBHPX_GATE_H_

#include "hpx/error.h"
#include "hpx/mem.h"
#include "hpx/lco.h"

static const uint64_t HPX_LCO_GATE_TYPE_AND      = UINT64_C(0);
static const uint64_t HPX_LCO_GATE_TYPE_OR       = UINT64_C(1);

/* gate options */
static const uint64_t HPX_LCO_GATE_MODE_SPARSE   = UINT64_C(0);
static const uint64_t HPX_LCO_GATE_MODE_DENSE    = UINT64_C(1);

static const uint64_t HPX_LCO_GATE_DATATYPE_PTR  = UINT64_C(0);
static const uint64_t HPX_LCO_GATE_DATATYPE_I8   = UINT64_C(1) << 1;
static const uint64_t HPX_LCO_GATE_DATATYPE_I16  = UINT64_C(1) << 2;
static const uint64_t HPX_LCO_GATE_DATATYPE_I32  = UINT64_C(1) << 3;
static const uint64_t HPX_LCO_GATE_DATATYPE_I64  = UINT64_C(1) << 4;
static const uint64_t HPX_LCO_GATE_DATATYPE_I128 = UINT64_C(1) << 5;
static const uint64_t HPX_LCO_GATE_DATATYPE_F32  = UINT64_C(1) << 6;
static const uint64_t HPX_LCO_GATE_DATATYPE_F64  = UINT64_C(1) << 7;
static const uint64_t HPX_LCO_GATE_DATATYPE_F80  = UINT64_C(1) << 8;
static const uint64_t HPX_LCO_GATE_DATATYPE_F128 = UINT64_C(1) << 9;

static const uint64_t HPX_LCO_GATE_HAS_PREDICATE = UINT64_C(1) << 63;

static const uint64_t HPX_LCO_GATE_DEFAULT_SIZE  = UINT64_C(32);


/*
 --------------------------------------------------------------------
  Gate Data Structures
 --------------------------------------------------------------------
*/

typedef void *      (*__hpx_gate_pred_t)      (void *,      void *);
typedef int8_t      (*__hpx_gate_pred_i8_t)   (int8_t,      void *);
typedef int16_t     (*__hpx_gate_pred_i16_t)  (int16_t,     void *);
typedef int32_t     (*__hpx_gate_pred_i32_t)  (int32_t,     void *);
typedef int64_t     (*__hpx_gate_pred_i64_t)  (int64_t,     void *);
typedef __m128i     (*__hpx_gate_pred_i128_t) (__m128i,     void *);
typedef float       (*__hpx_gate_pred_f32_t)  (float,       void *);
typedef double      (*__hpx_gate_pred_f64_t)  (double,      void *);
typedef long double (*__hpx_gate_pred_f80_t)  (long double, void *);
typedef __m128d     (*__hpx_gate_pred_f128_t) (__m128d,     void *);

typedef union _hpx_gate_pred_t {
  __hpx_gate_pred_t      vpf;
  __hpx_gate_pred_i8_t   i8f;
  __hpx_gate_pred_i16_t  i16f;
  __hpx_gate_pred_i32_t  i32f;
  __hpx_gate_pred_i64_t  i64f;
  __hpx_gate_pred_i128_t i128f;
  __hpx_gate_pred_f32_t  f32f;
  __hpx_gate_pred_f64_t  f64f;
  __hpx_gate_pred_f80_t  f80f;
  __hpx_gate_pred_f128_t f128f;
} hpx_gate_pred_t;

typedef struct _hpx_gate_t {
  uint8_t             gate_type;
  uint64_t            gate_opts;
  uint64_t            f_gen;
  uint64_t            g_gen;
  size_t              f_sz;
  hpx_future_t      **futures;
  hpx_mutex_t         mtx;
  hpx_gate_pred_t     pred;
  void               *userdata;
} hpx_gate_t __attribute__((aligned (8)));



/*
 --------------------------------------------------------------------
  hpx_lco_gate_init

  Initializes a gate to either an AND or an OR gate.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_gate_init(hpx_gate_t * gate, uint8_t g_type,
                                     uint64_t g_opts,
                                     void *(*f)(void *, void *),
                                     void * ud) {
  gate->gate_type = g_type;
  gate->gate_opts = g_opts;
  gate->f_gen = 0;
  gate->g_gen = 0;
  gate->f_sz = 0;
  gate->userdata = ud;

  /* set our predicate function (if any) */
  if (f != NULL) {
    if (g_opts & HPX_LCO_GATE_DATATYPE_I8) {
      gate->pred.i8f = (__hpx_gate_pred_i8_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_I16) {
      gate->pred.i16f = (__hpx_gate_pred_i16_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_I32) {
      gate->pred.i32f = (__hpx_gate_pred_i32_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_I64) {
      gate->pred.i64f = (__hpx_gate_pred_i64_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_I128) {
      gate->pred.i128f = (__hpx_gate_pred_i128_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_F32) {
      gate->pred.f32f = (__hpx_gate_pred_f32_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_F64) {
      gate->pred.f64f = (__hpx_gate_pred_f64_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_F80) {
      gate->pred.f80f = (__hpx_gate_pred_f80_t)f;
    } else if (g_opts & HPX_LCO_GATE_DATATYPE_F128) {
      gate->pred.f128f = (__hpx_gate_pred_f128_t)f;
    } else {
      gate->pred.vpf = (__hpx_gate_pred_t)f;
    }

    gate->gate_opts |= HPX_LCO_GATE_HAS_PREDICATE;
  } 

  /* initialize the gate mutex */
  hpx_lco_mutex_init(&gate->mtx, 0);

  /* allocate a buffer for futures */
  gate->futures = (hpx_future_t **) hpx_alloc(HPX_LCO_GATE_DEFAULT_SIZE * sizeof(hpx_future_t *));
  if (gate->futures == NULL) {
    __hpx_errno = HPX_ERROR_NOMEM;
    return;
  } else {
    gate->f_sz = HPX_LCO_GATE_DEFAULT_SIZE;
  }
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_create

  Creates and initializes a gate.
 --------------------------------------------------------------------
*/

static inline hpx_gate_t * hpx_lco_gate_create(uint8_t g_type, uint8_t g_opts,
                                               void *(*f)(void *, void *),
                                               void * ud) {
  hpx_gate_t * gate = NULL;

  /* allocate space */
  gate = (hpx_gate_t *) hpx_alloc(sizeof(hpx_gate_t));
  if (gate == NULL) {
    goto __hpx_lco_gate_create_FAIL0;
  }

  hpx_lco_gate_init(gate, g_type, g_opts, f, ud);

  return gate;

 __hpx_lco_gate_create_FAIL0:
  return NULL;
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_destroy

  Destroys (deallocates) a gate.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_gate_destroy(hpx_gate_t * gate) {
  hpx_future_t * fut;
  uint64_t idx;

  for (idx = 0; idx < gate->f_gen; idx++) {
    fut = gate->futures[idx];
    hpx_free(fut);
  }

  hpx_free(gate->futures);
  hpx_free(gate);
  gate = 0;
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_get_gen

  Gets the current generation number from a gate.
 --------------------------------------------------------------------
*/

static inline uint64_t hpx_lco_gate_get_gen(hpx_gate_t * gate) {
  return gate->g_gen;
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_get_future

  Obtains a future from a gate, at a specified generation.
 --------------------------------------------------------------------
*/

static inline hpx_future_t * hpx_lco_gate_get_future(hpx_gate_t * gate, size_t inp, uint64_t * gen) {
  hpx_future_t ** futures;
  hpx_future_t * g_fut;
  void ** values;
  size_t buf_sz;

  /* enter the critical section */
  hpx_lco_mutex_lock(&gate->mtx);

  /* see if we need to resize the futures buffer */
  if (gate->f_gen >= gate->f_sz) {
    futures = (hpx_future_t **) hpx_realloc(gate->futures, gate->f_sz * 2 * sizeof(hpx_future_t *));
    if (futures == NULL) {
      goto __hpx_lco_gate_get_future_FAIL0;
    } else {
      gate->futures = futures;
      gate->f_sz *= 2;
    }
  }

  /* allocate & initialize a new future */
  g_fut = (hpx_future_t *) hpx_alloc(sizeof(hpx_future_t *));
  if (g_fut == NULL) {
    goto __hpx_lco_gate_get_future_FAIL0;
  }

  /* initialize the future & set the input counter */
  hpx_lco_future_init(g_fut);
  g_fut->state = (inp & ~(HPX_LCO_FUTURE_SETMASK)); /* mask off the set bit */
   
  /* figure out how much space we need for the future's value */
  if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I8) {
    buf_sz = sizeof(int8_t);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I16) {
    buf_sz = sizeof(int16_t);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I32) {
    buf_sz = sizeof(int32_t);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I64) {
    buf_sz = sizeof(int64_t);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I128) {
    buf_sz = sizeof(__m128i);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F32) {
    buf_sz = sizeof(float);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F64) {
    buf_sz = sizeof(double);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F80) {
    buf_sz = sizeof(long double);
  } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F128) {
    buf_sz = sizeof(__m128d);
  } else {
    buf_sz = sizeof(void *);
  }

  /* allocate & initialize the future's value */
  if (gate->gate_opts & HPX_LCO_GATE_MODE_DENSE) {
    values = (void **) hpx_alloc(inp * buf_sz);

    if (values == NULL) {
      goto __hpx_lco_gate_get_future_FAIL1;
    } else {
      memset(values, 0, inp * buf_sz);
      g_fut->value.vp = (void *) values;
    }
  }

  /* set our generation counters */
  *gen = gate->f_gen;
  gate->f_gen += 1;

  /* set the future in the appropriate slot in the gate */
  gate->futures[*gen] = g_fut;

  /* leave the critical section */
  hpx_lco_mutex_unlock(&gate->mtx);

  return g_fut;

 __hpx_lco_gate_get_future_FAIL1:
  hpx_free(g_fut);
  g_fut = NULL;

 __hpx_lco_gate_get_future_FAIL0:
  hpx_lco_mutex_unlock(&gate->mtx);

  return NULL;
}


/*
 --------------------------------------------------------------------
  __hpx_lco_gate_wait_sync

  Internal wait function for gate generation counter
  synchronization.
 --------------------------------------------------------------------
*/

static inline bool __hpx_lco_gate_wait_sync(void * target, void * arg) {
  hpx_gate_t * gate = (hpx_gate_t *) target;
  uint64_t gen = (uint64_t) arg;
  bool sync = false;

  if (gen <= gate->g_gen) {
    sync = true;
  }
 
  return sync;
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_sync

  Synchronizes a gate with the requested generation number.  That
  is, the calling thread is suspended until the gate reaches the
  generation number that was requested.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_gate_sync(hpx_gate_t * gate, uint64_t gen) {
  if (__hpx_lco_gate_wait_sync((void *) gate, (void *) gen) == false) {
    _hpx_thread_wait((void *) gate, __hpx_lco_gate_wait_sync, (void *) gen);
  }
}


/*
 --------------------------------------------------------------------
  hpx_lco_gate_set

  Sets an element on a future at the current generation on a gate.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_gate_set(hpx_gate_t * gate) {
  hpx_future_t * g_fut;
  bool is_set = false;

  /* if we haven't allocated a future yet, just return */
  if (gate->futures == NULL) {
    return;
  }

  /* enter the critical section */
  hpx_lco_mutex_lock(&gate->mtx);
 
  /* get the future at the current generation and decrement the input counter */
  g_fut = gate->futures[gate->g_gen];

  hpx_lco_mutex_lock(&g_fut->mtx);
  g_fut->state -= 1;

  /* if there is a gate predicate, call it each time if the gate is SPARSE */
  if ((gate->gate_opts & HPX_LCO_GATE_MODE_SPARSE) && (gate->gate_opts & HPX_LCO_GATE_HAS_PREDICATE)) {
    if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I8) {
      g_fut->value.i8 = gate->pred.i8f(g_fut->value.i8, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I16) {
      g_fut->value.i16 = gate->pred.i16f(g_fut->value.i16, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I32) {
      g_fut->value.i32 = gate->pred.i32f(g_fut->value.i32, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I64) {
      g_fut->value.i64 = gate->pred.i64f(g_fut->value.i64, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I128) {
      g_fut->value.i128 = gate->pred.i128f(g_fut->value.i128, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F32) {
      g_fut->value.f32 = gate->pred.f32f(g_fut->value.f32, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F64) {
      g_fut->value.f64 = gate->pred.f64f(g_fut->value.f64, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F80) {
      g_fut->value.f80 = gate->pred.f80f(g_fut->value.f80, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F128) {
      g_fut->value.f128 = gate->pred.f128f(g_fut->value.f128, gate->userdata);
    } else {
      g_fut->value.vp = gate->pred.vpf(g_fut->value.vp, gate->userdata);
    }
  }

  /* see if we need to trigger the future */
  if ((gate->gate_type == HPX_LCO_GATE_TYPE_AND) && (g_fut->state == 0)) {
    is_set = true;
  } else if (gate->gate_type == HPX_LCO_GATE_TYPE_OR) {
    is_set = true;
  }

  /* if there is a gate predicate, call it when the future is ready if the gate is DENSE */
  if ((is_set == true) && (gate->gate_opts & HPX_LCO_GATE_MODE_DENSE) && (gate->gate_opts & HPX_LCO_GATE_HAS_PREDICATE)) {
    if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I8) {
      gate->pred.i8f(g_fut->value.i8, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I16) {
      gate->pred.i16f(g_fut->value.i16, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I32) {
      gate->pred.i32f(g_fut->value.i32, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I64) {
      gate->pred.i64f(g_fut->value.i64, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_I128) {
      gate->pred.i128f(g_fut->value.i128, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F32) {
      gate->pred.f32f(g_fut->value.f32, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F64) {
      gate->pred.f64f(g_fut->value.f64, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F80) {
      gate->pred.f80f(g_fut->value.f80, gate->userdata);
    } else if (gate->gate_opts & HPX_LCO_GATE_DATATYPE_F128) {
      gate->pred.f128f(g_fut->value.f128, gate->userdata);
    } else {
      gate->pred.vpf(g_fut->value.vp, gate->userdata);
    }
  }

  if (is_set == true) {
    g_fut->state |= HPX_LCO_FUTURE_SETMASK;
  } 

  hpx_lco_mutex_unlock(&g_fut->mtx);

  /* if the future was triggered, increment the gate's generation counter */
  if (is_set) {
    gate->g_gen += 1;
  }

  /* leave the critical section */
  hpx_lco_mutex_unlock(&gate->mtx);
}

#endif /* LIBHPX_GATE_H_ */

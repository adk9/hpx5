/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Mutual Exclusion (Mutex) Function Definitions
  mutex.h

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
#ifndef HPX_MUTEX_H_
#define HPX_MUTEX_H_

#include "hpx/error.h"
#include "hpx/mem.h"                            /* hpx_{alloc,free} */

#define HPX_LCO_MUTEX_OPT_RECURSIVE                              0x01
#define HPX_LCO_MUTEX_OPT_INITLOCKED                             0x02

#define HPX_LCO_MUTEX_SETMASK                                    0xFF

/*
 --------------------------------------------------------------------
  Mutex Data Structures
 --------------------------------------------------------------------
*/

typedef struct hpx_mutex {
  uint8_t __mtx;
  uint8_t __opt;
} hpx_mutex_t;


/*
 --------------------------------------------------------------------
  hpx_lco_mutex_init

  Initializes a mutex.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_mutex_init(hpx_mutex_t * mtx, uint8_t opts) {
  mtx->__mtx = 0x00;
  mtx->__opt = opts;
}


/*
 --------------------------------------------------------------------
  hpx_lco_mutex_create

  Allocates and initializes a mutex.
 --------------------------------------------------------------------
*/

static inline hpx_mutex_t * hpx_lco_mutex_create(uint8_t opts) {
  hpx_mutex_t * mtx = NULL;

  mtx = hpx_alloc(sizeof(hpx_mutex_t));
  if (mtx != NULL) {
    hpx_lco_mutex_init(mtx, opts);
  }

  return mtx;
}


/*
 --------------------------------------------------------------------
  hpx_lco_mutex_destroy

  Destroys a mutex previously created with hpx_lco_mutex_create.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_mutex_destroy(hpx_mutex_t * mtx) {
  hpx_free(mtx);
  mtx = NULL;
}


/*
 --------------------------------------------------------------------
  hpx_lco_mutex_lock

  Locks a mutex.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_mutex_lock(hpx_mutex_t * mtx) {
#ifdef __x86_64__
  __asm__ __volatile__(
    "__HLML%=:\n\t"
    "xorb %%al,%%al;\n\t"
    "movb %1,%%dl;\n\t"
    "lock; cmpxchg %%dl,%0;\n\t"
    "jnz __HLML%=;\n\t"
    :"=m" (mtx->__mtx)
    :"i" (HPX_LCO_MUTEX_SETMASK)
    :"%al", "%dl");
#endif
}


/*
 --------------------------------------------------------------------
  hpx_lco_mutex_unlock

  Unlocks a mutex.
 --------------------------------------------------------------------
*/

static inline void hpx_lco_mutex_unlock(hpx_mutex_t * mtx) {
#ifdef __x86_64__
  __asm__ __volatile__(
    "xorb %%al,%%al;\n\t"
    "lock; xchgb %%al,%0;\n\t"
    :"=m" (mtx->__mtx)
    :
    :"%al");
#endif
}

#endif /* HPX_MUTEX_H_ */

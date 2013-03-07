
/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  "Kernel" Thread Function Definitions
  hpx_kthread.h

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
#ifndef LIBHPX_KTHREAD_H_
#define LIBHPX_KTHREAD_H_

#include <stdint.h>
#include <pthread.h>


/*
 --------------------------------------------------------------------
  Kernel Thread Data
 --------------------------------------------------------------------
*/

typedef struct {
  pthread_t th;
} hpx_kthread_t;

typedef void *(*hpx_kthread_seed_t)(void *);


/*
 --------------------------------------------------------------------
  Seed Function
 --------------------------------------------------------------------
*/

void * hpx_kthread_seed_default(void *);


/*
 --------------------------------------------------------------------
  Kernel Thread Functions
 --------------------------------------------------------------------
*/

hpx_kthread_t * hpx_kthread_create(hpx_kthread_seed_t);
uint16_t hpx_kthread_get_affinity(hpx_kthread_t *);
void hpx_kthread_set_affinity(hpx_kthread_t *, uint16_t);
void hpx_kthread_destroy(hpx_kthread_t *);


/*
 --------------------------------------------------------------------
  Support Functions
 --------------------------------------------------------------------
*/

long hpx_kthread_get_cores(void);

#endif

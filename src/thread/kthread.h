/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Function Definitions
  hpx_thread.h

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
#ifndef LIBHPX_THREAD_KTHREAD_H_
#define LIBHPX_THREAD_KTHREAD_H_

/*
 --------------------------------------------------------------------
  Kernel Thread Functions
 --------------------------------------------------------------------
*/
void libhpx_kthread_sched(hpx_kthread_t *, hpx_thread_t *, uint8_t, void *,
                              bool (*)(void *, void *), void *);
void libhpx_kthread_push_pending(hpx_kthread_t *, hpx_thread_t *);

void libhpx_kthread_init(void);

/*
 --------------------------------------------------------------------
  Service Thread Functions
 --------------------------------------------------------------------
*/
void libhpx_kthread_srv_susp_local(void *);
void libhpx_kthread_srv_susp_global(void *);
void libhpx_kthread_srv_rebal(void *);



#endif /* LIBHPX_THREAD_KTHREAD_H_ */

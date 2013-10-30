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

#include <stdbool.h>
#include <stdint.h>

/**
 * @file
 * @brief Define the shared interface for kernel threads.
 */

/**
 * Forward declarations of the struct type.
 * @{
 */
struct hpx_kthread;
struct hpx_thread;
/**
 * @}
 */

/**
 * Schedule a thread.
 *
 * @param[in] kthread
 * @param[in] thread
 * @param[in] state
 * @param[in] target
 * @param[in] pred
 * @param[in] arg
 */
void libhpx_kthread_sched(struct hpx_kthread *kthread,
                          struct hpx_thread *thread,
                          uint8_t state,
                          void *target,
                          bool (*pred)(void *, void *),
                          void *arg);

void libhpx_kthread_push_pending(struct hpx_kthread *kthread,
                                 struct hpx_thread *thread);

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

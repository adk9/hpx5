/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Legacy interface used to "join" a thread.
  src/thread/join.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Luke Dalessandro <ldalessa [at] indiana.edu>
  ====================================================================
*/

#ifndef LIBHPX_THREAD_JOIN_H_
#define LIBHPX_THREAD_JOIN_H_

#include "hpx/system/attributes.h"

struct hpx_thread;

/**
 * Wait for a thread to terminate.
 *
 * @param[in]  thread
 * @param[out] result - the address of a location that will be set to the value
 *                      passed by thread to hpx_thread_exit().
 */
void thread_join(struct hpx_thread *thread, void **result)
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL);

#endif /* LIBHPX_THREAD_JOIN_H_ */

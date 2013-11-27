/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Thread Scheduling Context Function Definitions
  libhpx/ctx.h

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#ifndef LIBHPX_CONTEXT_H_
#define LIBHPX_CONTEXT_H_

struct hpx_context;

void ctx_add_kthread_init(struct hpx_context *ctx, void (*callback)(void))
  HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

void ctx_add_kthread_fini(struct hpx_context *ctx, void (*callback)(void))
    HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL,
                HPX_NON_NULL(1));

#endif /* LIBHPX_CONTEXT_H_ */

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

#include "hpx/system/attributes.h"

struct hpx_context;

HPX_INTERNAL void ctx_add_kthread_init(struct hpx_context *ctx,
                                       void (*callback)(void *),
                                       void *data)
  HPX_ATTRIBUTE(HPX_NON_NULL(1, 2));

HPX_INTERNAL void ctx_add_kthread_fini(struct hpx_context *ctx,
                                       void (*callback)(void *),
                                       void *data)
  HPX_ATTRIBUTE(HPX_NON_NULL(1, 2));

HPX_INTERNAL void ctx_start(struct hpx_context *ctx)
  HPX_ATTRIBUTE(HPX_NON_NULL(1));

#endif /* LIBHPX_CONTEXT_H_ */

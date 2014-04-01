// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_SYSTEM_H
#define LIBHPX_SYSTEM_H

#include "hpx/attributes.h"

struct thread;

HPX_INTERNAL int system_get_cores(void);
HPX_INTERNAL int system_set_affinity(struct thread *thread, int core_id);
HPX_INTERNAL void system_shutdown(int code);

#endif // LIBHPX_SYSTEM_H

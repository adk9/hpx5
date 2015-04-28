// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================

// pthread barrier not supported in Mac. Conditionally selecting barrier implementation
// for mac.
#ifndef HPX_PTHREAD_BARRIER
#define HPX_PTHREAD_BARRIER
#ifdef __APPLE__
#include "pthread_darwin.h"
#else
#include <pthread.h>
#endif
#endif

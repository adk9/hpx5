/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Platform tests that don't require configuration-time details.
  platform.h

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

#pragma once

/**
 * @macro HPX_ALIGNED(i)
 */
#ifdef __GNUC__
#define HPX_ALIGNED(i) __attribute__ ((aligned(i)))
#endif

/**
 * @macro HPX_NON_NULL(i)
 */
#ifdef __GNUC__
#define HPX_NON_NULL(...) __attribute__ ((nonnull(__VA_ARGS__)))
#endif

/**
 * @macro HPX_RETURNS_NON_NULL()
 */
#ifdef __GNUC__
#define HPX_RETURNS_NON_NULL()
/* __attribute__ ((returns_nonnull)) */
#endif

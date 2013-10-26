/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Platform-specific code used by public hpx headers.
  
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

#ifndef HPX_PLATFORM_H_
#define HPX_PLATFORM_H_

/**
 * @file
 * @brief Platform-specific macros that depend on architecture or compiler, and
 *        are used by public headers---also usable by libhpx implementations.
 */

/**
 * @macro HPX_ATTRIBUTE(...)
 * @brief Abstract away the specific attribute syntax for the compiler.
 */
#ifdef __GNUC__
#define HPX_ATTRIBUTE(...) __attribute__ ((__VA_ARGS__))
#else
#warning Unknown attribute syntax for your compiler. This can case problems.
#warning Make sure to run `make check` on your system.
#define HPX_ATTRIBUTE(...)
#endif

/**
 * @macro HPX_ALIGNED(i)
 * @brief The attribute for aligning data.
 */
#ifdef __GNUC__
#define HPX_ALIGNED(i) aligned(i)
#else
#warning Unknown attribute "aligned" for your compiler. This can cause layout
#warning problems, be sure to run `make check`.
#define HPX_ALIGNED(i)
#endif

/**
 * @macro HPX_NON_NULL(i)
 * @brief The attribute for declaring that a pointer parameter must not be
 *        NULL. No problem if it's not available.
 */
#ifdef __GNUC__
#define HPX_NON_NULL(...) nonnull(__VA_ARGS__)
#else
#define HPX_NON_NULL(...)
#endif

/**
 * @macro HPX_RETURNS_NON_NULL
 * @brief The attribute that declares that a function returns a pointer
 *        guaranteed to be non-NULL. No problem if it's not available.
 */
#ifdef __GNUC__
#define HPX_RETURNS_NON_NULL /* __attribute__ ((returns_nonnull)) */
#else
#define HPX_RETURNS_NON_NULL
#endif

/**
 * @macro HPX_VISIBILITY_INTERNAL
 * @brief The addtribute that declares that a symbol should have "hidden"
 *        visibility. Not a problem if it isn't available.
 */
#ifdef __GNUC__
#define HPX_VISIBILITY_INTERNAL visibility("hidden")
#else
#define HPX_VISIBILITY_INTERNAL
#endif

#endif /* HPX_PLATFORM_H_ */

/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Platform-specific attribute code used by hpx.
  
  include/hpx/system/attributes.h

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

#ifndef HPX_SYSTEM_ATTRIBUTES_H_
#define HPX_SYSTEM_ATTRIBUTES_H_

/**
 * @file
 * @brief Platform-specific macros that depend on architecture or compiler, and
 *        are used by public headers---also usable by libhpx implementations.
 */

/**
 * @macro HPX_ATTRIBUTE
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
 * @macro HPX_ALIGNED
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
 * @macro HPX_NON_NULL
 * @brief The attribute for declaring that a pointer parameter must not be
 *        NULL.
 *
 * No problem if it's not available.
 */
#ifdef __GNUC__
#define HPX_NON_NULL(...) nonnull(__VA_ARGS__)
#else
#define HPX_NON_NULL(...)
#endif

/**
 * @macro HPX_RETURNS_NON_NULL
 * @brief The attribute that declares that a function returns a pointer
 *        guaranteed to be non-NULL.
 *
 * No problem if it's not available.
 */
#ifdef __GNUC__
#define HPX_RETURNS_NON_NULL /* __attribute__ ((returns_nonnull)) */
#else
#define HPX_RETURNS_NON_NULL
#endif

/**
 * @macro HPX_VISIBILITY_INTERNAL
 * @brief The attribute that declares that a symbol should have "hidden"
 *        visibility.
 *
 * Not a problem if it isn't available.
 */
#ifdef __GNUC__
#define HPX_VISIBILITY_INTERNAL visibility("hidden")
#else
#define HPX_VISIBILITY_INTERNAL
#endif

/**
 * @macro HPX_INTERNAL
 * @brief Convenience wrapper for the internal visibility attribute
 */
#define HPX_INTERNAL HPX_ATTRIBUTE(HPX_VISIBILITY_INTERNAL)

/**
 * @macro HPX_MALLOC
 * @brief The attribute that declares that a function has "malloc" semantics
 *        w.r.t. the aliasing behavior of the returned pointer value.
 *
 * Not a problem if it is unavailable.
 */
#ifdef __GNUC__
#define HPX_MALLOC malloc
#else
#define HPX_MALLOC
#endif

/**
 * @macro HPX_NORETURN
 * @brief The attribute that declares that a function does not return.
 *
 * Not a problem if it is unavailable.
 */
#ifdef __GNUC__
#define HPX_NORETURN noreturn
#else
#define HPX_NORETURN
#endif

/**
 * @macro HPX_ALWAYS_INLINE
 * @brief The attribute that declares that a function should always be inlined.
 *
 * Not a problem if it is unavailable.
 */
#ifdef __GNUC__
#define HPX_ALWAYS_INLINE always_inline
#else
#define HPX_ALWAYS_INLINE
#endif

/**
 * @macro HPX_ALLOC_SIZE
 * @brief The attribute that declares that a function returns a pointer to a
 *        specific size chunk of memory.
 *
 * Not a problem if it is unavailable.
 */
#ifdef __GNUC__
#define HPX_ALLOC_SIZE(...) alloc_size(__VA_ARGS__)
#else
#define HPX_ALLOC_SIZE(...)
#endif

/**
 * @macro HPX_RETURNS_TWICE
 * @brief The attribute that declares that a function returns twice
 *        (setjmp-style).
 */
#ifdef __GNUC__
#define HPX_RETURNS_TWICE returns_twice
#else
#define HPX_RETURNS_TWICE
#endif

#endif /* HPX_SYSTEM_ATTRIBUTES_H_ */

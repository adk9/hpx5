/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Debug-dependent definitions.
  
  include/libhpx/debug.h

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

#ifndef HPX_DEBUG_H_
#define HPX_DEBUG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DEBUG
#include <assert.h>
#include <stdio.h>
#include <hpx/system/abort.h>
#endif

/**
 * @brief dbg_printf 
 */
#ifdef HAVE_DEBUG
#define dbg_printf(...) printf(__VA_ARGS__)
#else
#define dbf_printf(...)
#endif

/**
 * @brief dbg_print_error 
 */
#ifdef HAVE_DEBUG
#define dbg_print_error(...)                    \
  do {                                          \
    fprintf(stderr, __VA_ARGS__);               \
    hpx_abort();                                \
  } while (0)
#else
#define dbg_print_error(...)
#endif

/**
 * @brief dbg_assert
 */
#ifdef HAVE_DEBUG
#define dbg_assert(statement) assert(statement)
#else
#define dbg_assert(statement)
#endif

/**
 * @brief dbg_assert
 */
#ifdef HAVE_DEBUG
#define dbg_assert_precondition(check) \
  assert(check && "Precondition check failed")
#else
#define dbg_assert_precondition(check)
#endif

#endif /* HPX_DEBUG_H_ */

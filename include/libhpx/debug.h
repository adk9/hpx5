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

#ifdef ENABLE_DEBUG
#include <assert.h>
#include <stdio.h>
#include "hpx/error.h"
#include "hpx/system/abort.h"
#endif

/**
 * @brief dbg_printf 
 */
#ifdef ENABLE_DEBUG
#define dbg_printf(...)                         \
  do {                                          \
    printf(__VA_ARGS__);                        \
    fflush(stdout);                             \
  } while (0)
#else
#define dbg_printf(...)
#endif

/**
 * @brief dbg_print_error 
 */
#ifdef ENABLE_DEBUG
#define dbg_print_error(e, ...)                 \
  do {                                          \
    fprintf(stderr, __VA_ARGS__);               \
    fflush(stderr);                             \
    hpx_abort();                                \
  } while (0)
#else
#define dbg_print_error(...)
#endif

/**
 * @brief dbg_assert
 */
#ifdef ENABLE_DEBUG
#define dbg_assert(statement) assert(statement)
#else
#define dbg_assert(statement)
#endif

/**
 * @brief dbg_assert
 */
#ifdef ENABLE_DEBUG
#define dbg_assert_precondition(check) \
  assert(check && "Precondition check failed")
#else
#define dbg_assert_precondition(check)
#endif

/**
 * @brief dbg_check_success
 *
 * When debugging is enabled, this ensures that the result of a function is
 * HPX_SUCCESS. When it fails, it uses dbg_print_error to abort.
 */
#ifdef ENABLE_DEBUG
#define dbg_check_success(check)                                        \
  do {                                                                  \
    if ((hpx_error_t e = (check)) != HPX_SUCCESS)                       \
      dbg_print_error(e, "Unhandled error found during dbg_check_success"); \
  } while (0);
#else
#define dbg_check_success(check) check
#endif

/**
 * @brief
 */
#ifdef ENABLE_DEBUG
#define HPX_DEBUG 1
#else
#define HPX_DEBUG 0
#endif

#endif /* HPX_DEBUG_H_ */

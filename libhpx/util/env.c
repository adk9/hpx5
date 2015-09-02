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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/util/env.c
/// @brief Utility functions for dealing with the environment.
///

#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

#include <libhpx/debug.h>
#include <libhpx/utils.h>

/// Getenv, but with an upper-case version of @p key.
static char *_getenv_upper(const char * const key) {
  char *c = NULL;
  const size_t len = strlen(key);
  char *uvar = malloc(len + 1);
  dbg_assert_str(uvar, "Could not malloc %zu bytes during getenv", len);
  for (int i = 0; i < len; ++i) {
    uvar[i] = toupper(key[i]);
  }
  uvar[len] = '\0';
  c = getenv(uvar);
  free(uvar);
  return c;
}

/// Get a value from a environment variable @p key.
char *libhpx_getenv(const char * const key) {
  char *c = getenv(key);
  if (!c) {
    c = _getenv_upper(key);
  }
  if (!c) {
    return NULL;
  }
  return c;
}


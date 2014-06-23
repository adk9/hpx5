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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/scheduler/schedule.c
/// ----------------------------------------------------------------------------
#include <stdint.h>

#include "cvar.h"
#include "thread.h"

static const int _CODE_OFFSET = ((sizeof(uintptr_t) / 2) * 8);
static const uintptr_t _ERROR_MASK = 0x1;

// static check to make sure the error code is related to the cvar size in the
// way that we expect---if this fails we're probably on a 32-bit platform, and
// we need a different error code size
static HPX_UNUSED int check[sizeof(cvar_t) - (2 * sizeof(hpx_status_t)) + 1];

static uintptr_t
_has_error(const cvar_t *cvar) {
  return (uintptr_t)cvar->top & _ERROR_MASK;
}

ustack_t *
cvar_set_error(cvar_t *cvar, hpx_status_t code)
{
  if (_has_error(cvar))
    return NULL;

  ustack_t *top = cvar->top;
  cvar->top = (ustack_t*)((((uintptr_t)code) << _CODE_OFFSET) | _ERROR_MASK);
  return top;
}


hpx_status_t
cvar_get_error(const cvar_t *cvar)
{
  if (_has_error(cvar)) {
    return (hpx_status_t)((uintptr_t)cvar >> _CODE_OFFSET);
  }
  else {
    return HPX_SUCCESS;
  }
}

void
cvar_clear_error(cvar_t *cvar)
{
  if (_has_error(cvar))
    cvar->top = NULL;
}


hpx_status_t
cvar_push_thread(cvar_t *cvar, struct ustack *thread)
{
  if (_has_error(cvar))
    return cvar_get_error(cvar);

  thread->next = cvar->top;
  cvar->top = thread;
  return HPX_SUCCESS;
}

ustack_t *
cvar_pop_thread(cvar_t *cvar)
{
  if (_has_error(cvar))
    return NULL;

  ustack_t *thread = cvar->top;
  if (thread)
    cvar->top = thread->next;
  return thread;
}


ustack_t *
cvar_pop_all(cvar_t *cvar)
{
  if (_has_error(cvar))
    return NULL;

  ustack_t *thread = cvar->top;
  cvar->top = NULL;
  return thread;
}

void
cvar_reset(cvar_t *cvar)
{
  cvar->top = NULL;
}

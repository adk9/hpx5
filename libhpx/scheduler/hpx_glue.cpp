// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include "hpx/hpx.h"
#include "thread.h"
#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/parcel.h"
#include "libhpx/c_scheduler.h"
#include "libhpx/Worker.h"

hpx_parcel_t *
_hpx_thread_generate_continuation(int n, ...)
{
  hpx_parcel_t *p = self->current;

  dbg_assert(p->ustack->cont == 0);

  hpx_action_t op = p->c_action;
  hpx_addr_t target = p->c_target;
  va_list args;
  va_start(args, n);
  hpx_parcel_t *c = action_new_parcel_va(op, target, 0, 0, n, &args);
  va_end(args);

  p->ustack->cont = 1;
  p->c_action = 0;
  p->c_target = 0;
  return c;
}

void
hpx_exit(size_t size, const void *out)
{
  assert(here && self);
  scheduler_exit(here->sched, size, out);
}

void
hpx_thread_yield(void)
{
  scheduler_yield();
}

int
hpx_get_my_thread_id(void)
{
  Worker *w = self;
  return (w) ? w->id : -1;
}


const hpx_parcel_t*
hpx_thread_current_parcel(void)
{
  Worker *w = self;
  return (w) ? w->current : NULL;
}

hpx_addr_t
hpx_thread_current_target(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->target : HPX_NULL;
}

hpx_addr_t
hpx_thread_current_cont_target(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->c_target : HPX_NULL;
}

hpx_action_t
hpx_thread_current_action(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->action : HPX_ACTION_NULL;
}

hpx_action_t
hpx_thread_current_cont_action(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->c_action : HPX_ACTION_NULL;
}

hpx_pid_t
hpx_thread_current_pid(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->pid : HPX_NULL;
}

uint32_t
hpx_thread_current_credit(void)
{
  Worker *w = self;
  return (w && w->current) ? w->current->credit : 0;
}

int
hpx_is_active(void)
{
  return (self->current != NULL);
}

void
hpx_thread_exit(int status)
{
  throw status;
}

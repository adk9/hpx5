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

#include "Condition.h"
#include "TatasLock.h"
#include "libhpx/events.h"
#include "libhpx/libhpx.h"
#include "libhpx/Worker.h"
#include <memory>

namespace {
using libhpx::self;
using Condition = libhpx::scheduler::Condition;
using Mutex = libhpx::scheduler::TatasLock<short>;
}

int
libhpx_mutex_init(libhpx_mutex_t* mutex) {
  new (mutex) Mutex{};
  return LIBHPX_OK;
}

int
libhpx_mutex_destroy(libhpx_mutex_t* mutex)
{
  reinterpret_cast<Mutex*>(mutex)->~Mutex();
  return LIBHPX_OK;
}

int
libhpx_mutex_lock(libhpx_mutex_t* mutex)
{
  reinterpret_cast<Mutex*>(mutex)->lock();
  return LIBHPX_OK;
}

int
libhpx_mutex_unlock(libhpx_mutex_t* mutex)
{
  reinterpret_cast<Mutex*>(mutex)->unlock();
  return LIBHPX_OK;
}

int
libhpx_cond_init(libhpx_cond_t* cond)
{
  new (cond) Condition{};
  return LIBHPX_OK;
}

int
libhpx_cond_destroy(libhpx_cond_t* cond)
{
  reinterpret_cast<Condition*>(cond)->~Condition();
  return LIBHPX_OK;
}

int
libhpx_cond_wait(libhpx_cond_t* cond, libhpx_mutex_t* mutex)
{
  auto* w = self;
  Condition* c = reinterpret_cast<Condition*>(cond);
  Mutex* m = reinterpret_cast<Mutex*>(mutex);
  hpx_parcel_t* p = w->getCurrentParcel();

  if (hpx_status_t status = c->push(p)) {
    return status;
  }

  w->EVENT_THREAD_SUSPEND(p);
  w->schedule([m](hpx_parcel_t* p) {
      m->unlock();
    });

  self->EVENT_THREAD_RESUME(p);                 // self is volatile
  m->lock();
  return c->getError();
}

int
libhpx_cond_signal(libhpx_cond_t* cond)
{
  reinterpret_cast<Condition*>(cond)->signal();
  return LIBHPX_OK;
}

int
libhpx_cond_broadcast(libhpx_cond_t* cond)
{
  reinterpret_cast<Condition*>(cond)->signalAll();
  return LIBHPX_OK;
}

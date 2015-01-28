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


/// Given a set of elements distributed across all processes, Allgather will
/// gather all of the elements to all the processes. It gathers and broadcasts
/// to all.
///
///           sendbuff
///           ########
///           #      #
///         0 #  A0  #
///           #      #
///           ########
///      T    #      #
///         1 #  B0  #
///      a    #      #
///           ########
///      s    #      #
///         2 #  C0  #                                   BEFORE
///      k    #      #
///           ########
///      s    #      #
///         3 #  D0  #
///           #      #
///           ########
///           #      #
///         4 #  E0  #
///           #      #
///           ########
///
///             <---------- recvbuff ---------->
///           ####################################
///           #      #      #      #      #      #
///         0 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///      T    #      #      #      #      #      #
///         1 #  A0  #  B0  #  C0  #  D0  #  E0  #
///      a    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         2 #  A0  #  B0  #  C0  #  D0  #  E0  #       AFTER
///      k    #      #      #      #      #      #
///           ####################################
///      s    #      #      #      #      #      #
///         3 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################
///           #      #      #      #      #      #
///         4 #  A0  #  B0  #  C0  #  D0  #  E0  #
///           #      #      #      #      #      #
///           ####################################


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/// @file libhpx/scheduler/allgather.c
/// @brief Defines the allgather LCO.
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

static const int _gathering = 0;
static const int _reading   = 1;

typedef struct {
  lco_t           lco;
  cvar_t         wait;
  size_t participants;
  size_t        count;
  volatile int  phase;
  void         *value;
} _allgather_t;


typedef struct {
  int offset;
  char buffer[];
} _allgather_set_offset_t;

static hpx_action_t _allgather_setid_action = 0;

/// Deletes a gathering.
static void _allgather_fini(lco_t *lco) {
  if (!lco)
    return;

  lco_lock(lco);
  DEBUG_IF(true) {
    lco_set_deleted(lco);
  }
  _allgather_t *g = (_allgather_t *)lco;
  if (g->value)
    free(g->value);
  libhpx_global_free(g);
}


/// Handle an error condition.
static void _allgather_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _allgather_t *g = (_allgather_t *)lco;
  scheduler_signal_error(&g->wait, code);
  lco_unlock(lco);
}

/// Get the value of the gathering, will wait if the phase is gathering.
static hpx_status_t _allgather_get(lco_t *lco, int size, void *out) {
  _allgather_t *g = (_allgather_t *)lco;
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);

  // wait until we're reading, and watch for errors
  while ((g->phase != _reading) && (status == HPX_SUCCESS))
    status = scheduler_wait(&g->lco.lock, &g->wait);

  // if there was an error signal, unlock and return it
  if (status != HPX_SUCCESS)
    goto unlock;

  // we're in a reading phase, if the user wants the data, copy it out
  if (size && out)
    memcpy(out, g->value, size);

  // update the count, if I'm the last reader to arrive, switch the mode and
  // release all of the other readers, otherwise wait for the phase to change
  // back to gathering---this blocking behavior prevents gets from one "epoch"
  // to satisfy earlier _reading epochs
  if (++g->count == g->participants) {
    g->phase = _gathering;
    scheduler_signal_all(&g->wait);
  }
  else {
    while ((g->phase == _reading) && (status == HPX_SUCCESS))
      status = scheduler_wait(&g->lco.lock, &g->wait);
  }

 unlock:
  lco_unlock(lco);
  return status;
}

// Wait for the gathering, loses the value of the gathering for this round.
static hpx_status_t _allgather_wait(lco_t *lco) {
  return _allgather_get(lco, 0, NULL);
}

// Local set id function.
static hpx_status_t _allgather_setid(_allgather_t *g, unsigned offset, int size,
                                     const void* buffer) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(&g->lco);

  // wait until we're gathering
  while ((g->phase != _gathering) && (status == HPX_SUCCESS))
    status = scheduler_wait(&g->lco.lock, &g->wait);

  if (status != HPX_SUCCESS)
    goto unlock;

  // copy in our chunk of the data
  assert(size && buffer);
  memcpy((char*)g->value + (offset * size), buffer, size);

  // if we're the last one to arrive, switch the phase and signal readers
  if (--g->count == 0) {
    g->phase = _reading;
    scheduler_signal_all(&g->wait);
  }

 unlock:
  lco_unlock(&g->lco);
  return status;
}

/// Set the ID for allgather. This is global setid for the user to use.
///
/// @param   allgather  Global address of the altogether LCO
/// @param   id         ID to be set
/// @param   size       The size of the data being gathered
/// @param   value      Address of the value to be set
/// @param   lsync      An LCO to signal on local completion HPX_NULL if we
///                     don't care. Local completion indicates that the
///                     @value may be freed or reused.
/// @param   rsync      An LCO to signal remote completion HPX_NULL if we
///                     don't care.
/// @returns HPX_SUCCESS or the code passed to hpx_lco_error()
hpx_status_t hpx_lco_allgather_setid(hpx_addr_t allgather, unsigned id,
                                     int size, const void *value,
                                     hpx_addr_t lsync, hpx_addr_t rsync)
{
  hpx_status_t status = HPX_SUCCESS;
  _allgather_t *local;

  if (!hpx_gas_try_pin(allgather, (void**)&local)) {
    size_t args_size = sizeof(_allgather_set_offset_t) + size;
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, args_size);
    assert(p);
    hpx_parcel_set_target(p, allgather);
    hpx_parcel_set_action(p, _allgather_setid_action);
    hpx_parcel_set_cont_target(p, rsync);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    _allgather_set_offset_t *args = hpx_parcel_get_data(p);
    args->offset = id;
    memcpy(&args->buffer, value, size);
    hpx_parcel_send(p, lsync);
  }
  else {
    status = _allgather_setid(local, id, size, value);
    if (lsync)
      hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    if (rsync)
      hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
  }

  return status;
}


static hpx_status_t _allgather_setid_proxy(void *args) {
  // try and pin the allgather LCO, if we fail, we need to resend the underlying
  // parcel to "catch up" to the moving LCO
  hpx_addr_t target = hpx_thread_current_target();
  _allgather_t *g;
  if(!hpx_gas_try_pin(target, (void **)&g))
     return HPX_RESEND;

  // otherwise we pinned the LCO, extract the arguments from @p args and use the
  // local setid routine
  _allgather_set_offset_t *a = args;
  size_t size = hpx_thread_current_args_size() - sizeof(_allgather_set_offset_t);
  hpx_status_t status = _allgather_setid(g, a->offset, size, &a->buffer);
  hpx_gas_unpin(target);
  return status;
}


static HPX_CONSTRUCTOR void _initialize_actions(void) {
  LIBHPX_REGISTER_ACTION(_allgather_setid_proxy, &_allgather_setid_action);
}

/// Update the gathering, will wait if the phase is reading.
static void
_allgather_set(lco_t *lco, int size, const void *from)
{
  // can't call set on an allgather
  hpx_abort();
}

static void _allgather_init(_allgather_t *g, size_t participants, size_t size) {
  // vtable
  static const lco_class_t vtable = {
    .on_fini = _allgather_fini,
    .on_error = _allgather_error,
    .on_set = _allgather_set,
    .on_attach = NULL,
    .on_get = _allgather_get,
    .on_wait = _allgather_wait,
    .on_try_get = NULL,
    .on_try_wait = NULL
  };

  lco_init(&g->lco, &vtable, 0);
  cvar_reset(&g->wait);
  g->participants = participants;
  g->count = participants;
  g->phase = _gathering;
  g->value = NULL;

  if (size) {
    // Ultimately, g->value points to start of the array containing the
    // gathered data.
    g->value = malloc(size * participants);
    assert(g->value);
  }
}

/// Allocate a new gather LCO. It gathers elements from each process in order
/// of their rank and sends the result to all the processes
///
/// The gathering is allocated in gathering-mode, i.e., it expects @p
/// participants to call the hpx_lco_allgather_setid() operation as the first
/// phase of operation.
///
/// @param participants The static number of participants in the gathering.
/// @param size         The size of the data being gathered.
hpx_addr_t hpx_lco_allgather_new(size_t inputs, size_t size) {
  _allgather_t *g = libhpx_global_malloc(sizeof(*g));
  assert(g);
  _allgather_init(g, inputs, size);
  return lva_to_gva(g);
}


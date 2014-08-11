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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

static const int _gathering = 0;
static const int _reading   = 1;

typedef struct {
  lco_t                           lco;
  cvar_t                         wait;
  size_t                 participants;
  size_t                        count;
  volatile int                  phase;
  void                         *value;   
} _allgather_t;

static hpx_action_t _allgather_setid_gen_action = 0;

hpx_status_t hpx_lco_local_setid(_allgather_set_offset_t *ag,
                                 lco_t *lco, hpx_addr_t lsync,
                                 hpx_addr_t rsync);

/// This utility gathers the count of waiting
static int
_allgather_join(_allgather_t *g)
{
  assert(g->count != 0);

  if (--g->count > 0)
    return 0;

  g->phase = 1 - g->phase;
  g->count = g->participants;
  scheduler_signal_all(&g->wait);
  return 1;
}

/// Deletes a gathering.
static void _allgather_fini(lco_t *lco)
{
  lco_lock(lco);
  _allgather_t *g = (_allgather_t *)lco;
  if (g->value)
    free(g->value);
  if (g)
    free(g);
}


/// Handle an error condition.
static void _allgather_error(lco_t *lco, hpx_status_t code)
{
  lco_lock(lco);
  _allgather_t *g = (_allgather_t *)lco;
  scheduler_signal_error(&g->wait, code);
  lco_unlock(lco);
}

/// Get the value of the gathering, will wait if the phase is gathering.
static hpx_status_t _allgather_get(lco_t *lco, int size, void *out)
{
  _allgather_t *g = (_allgather_t *)lco;
  lco_lock(lco);

  hpx_status_t status = cvar_get_error(&g->wait);

  // wait until we're reading, then read the value and join the gathering
  while ((g->phase != _reading) && (status == HPX_SUCCESS))
    status = scheduler_wait(&lco->lock, &g->wait);

  if (status == HPX_SUCCESS) {
    if (size && out)
      memcpy(out, g->value, size);
  }

  lco_unlock(lco);
  return status;
}

// Wait for the gathering, loses the value of the gathering for this round.
static hpx_status_t _allgather_wait(lco_t *lco)
{
  return _allgather_get(lco, 0, NULL);
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
hpx_status_t hpx_lco_allgather_setid(hpx_addr_t allgather, unsigned id, int
                                     size, const void *value, hpx_addr_t lsync,
                                     hpx_addr_t rsync) {
  hpx_status_t status = HPX_SUCCESS;
  lco_t *local;
  
  _allgather_set_offset_t args;
  args.size = size;
  args.offset = hpx_get_my_rank();
  memcpy(args.buffer, value, size);
  
  if(!hpx_gas_try_pin(allgather, (void**)&local)){
     return hpx_call_async(allgather, _allgather_setid_gen_action, 
               &args, size, lsync, rsync);
  }
  hpx_lco_local_setid(&args, local, lsync, rsync); 
  return status;
}

// Local set id function.
hpx_status_t hpx_lco_local_setid(_allgather_set_offset_t *ag, 
                                 lco_t *lco, hpx_addr_t lsync,
                                 hpx_addr_t rsync) {
  hpx_status_t status = HPX_SUCCESS;
  lco_lock(lco);
  
  _allgather_t *g = (_allgather_t *)lco;
  int offset = ag->offset;
  
  // wait until we're gathering, then perform the setid() 
  // and join the gathering
  while (g->phase != _gathering)
    scheduler_wait(&lco->lock, &g->wait);

  memcpy(ag->buffer + offset, g->value, ag->size);

  _allgather_join(g);
  lco_unlock(lco);
  return status;
}

static hpx_status_t _allgather_setid_gen_proxy(void *args)
{
  hpx_addr_t target = hpx_thread_current_target();
  lco_t *lco;
  if(!hpx_gas_try_pin(target, (void **)&lco))
  {
     return HPX_RESEND;
  }
  return hpx_lco_local_setid(args, lco, HPX_NULL, HPX_NULL);
}


static HPX_CONSTRUCTOR void
_initialize_actions(void)
{
  _allgather_setid_gen_action = HPX_REGISTER_ACTION(_allgather_setid_gen_proxy);
}

/// Update the gathering, will wait if the phase is reading.
static void
_allgather_set(lco_t *lco, int size, const void *from)
{
  //
}

static void _allgather_init(_allgather_t *g, size_t participants,
                size_t size)
{
  // vtable
  static const lco_class_t vtable = {
    _allgather_fini,
    _allgather_error,
    _allgather_set,
    _allgather_get,
    _allgather_wait
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
  hpx_addr_t gather = locality_malloc(sizeof(_allgather_t));
  _allgather_t *g = NULL;
  if (!hpx_gas_try_pin(gather, (void**)&g)) {
    dbg_error("Could not pin newly allocated gathering.\n");
    hpx_abort();
  }
  _allgather_init(g, inputs, size);
  hpx_gas_unpin(gather);
  return gather;
}


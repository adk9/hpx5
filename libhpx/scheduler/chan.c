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
/// @file libhpx/scheduler/chan.c
/// Defines a channel structure.
/// ----------------------------------------------------------------------------
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "libsync/queues.h"

#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "lco.h"


#define _QUEUE(pre, post) pre##two_lock_queue##post
//#define _QUEUE(pre, post) pre##ms_queue##post
#define _QUEUE_T _QUEUE(, _t)
#define _QUEUE_INIT _QUEUE(sync_, _init)
#define _QUEUE_FINI _QUEUE(sync_, _fini)
#define _QUEUE_ENQUEUE _QUEUE(sync_, _enqueue)
#define _QUEUE_DEQUEUE _QUEUE(sync_, _dequeue)


/// ----------------------------------------------------------------------------
/// Local channel interface.
/// ----------------------------------------------------------------------------
/// @{
typedef struct {
  lco_t    lco;                     // channel "is-an" lco
  _QUEUE_T buf;                     // channel buffer
} _chan_t;


/// Freelist allocation for chans.
static __thread _chan_t *_free = NULL;


/// Internal actions.
static hpx_action_t _send        = 0;
static hpx_action_t _recv        = 0;
static hpx_action_t _async_set   = 0;
static hpx_action_t _block_init  = 0;
static hpx_action_t _blocks_init = 0;


/// Deletes a channel and its internal buffers.
///
/// NB: deadlock issue here
static void _delete(_chan_t *c) {
  if (!c)
    return;

  lco_lock(&c->lco);
  _QUEUE_FINI(&c->buf);
  lco_fini(&c->lco);

  // overload the vtable pointer for freelisting---not perfect, but it's
  // reinitialized in _init(), so it's not the end of the world.
  c->lco.vtable = (lco_class_t*)_free;
  _free = c;
}


typedef struct {
  _chan_t      *chan;
  hpx_status_t  status;
  void         *src;
  int           size;
} _set_args_t;


static int _async_set_action(_set_args_t *args) {
  _chan_t *c = args->chan;

  void *buf = malloc(args->size);
  assert(buf);
  memcpy(buf, args->src, args->size);

  lco_lock(&c->lco);
  _QUEUE_ENQUEUE(&c->buf, buf);

  if (!lco_is_set(&c->lco))
    scheduler_signal(&c->lco, args->status);

  lco_unlock(&c->lco);
  hpx_thread_continue(0, NULL);
}

/// Copies the @p from pointer into channel's buffer. This is
/// equivalent to the send operation on a channel.
static void _set(_chan_t *c, int size, const void *from, hpx_status_t status, hpx_addr_t sync)
{
  assert(from);
  _set_args_t args = { .chan = c, .status = status,
                       .src = (void*)from, .size = size };
  hpx_call(HPX_HERE, _async_set, &args, sizeof(_set_args_t), sync);
}


/// Copies the appropriate pointer from channel's buffer into @p out,
/// waiting if the channel buffer is empty. This is equivalent to a
/// receive operation on a channel.
static hpx_status_t _get(_chan_t *c, int size, void *out) {
  assert(out);

  lco_lock(&c->lco);
  void *ptr = NULL;
  do {
    ptr = _QUEUE_DEQUEUE(&c->buf);
    if (ptr == NULL) {
      lco_reset(&c->lco);
      scheduler_wait(&c->lco);
    }
  } while (ptr == NULL);

  memcpy(out, &ptr, sizeof(ptr));

  hpx_status_t status = lco_get_status(&c->lco);
  lco_unlock(&c->lco);
  return status;
}


/// The channel vtable.
static lco_class_t _vtable = LCO_CLASS_INIT(_delete, _set, _get);


/// Initialize the channel
static void _init(_chan_t *c) {
  lco_init(&c->lco, &_vtable, 0);
  _QUEUE_INIT(&c->buf, NULL);
}


static int _send_action(void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_addr_t cont = hpx_thread_current_cont();
  _chan_t *chan;
  if (!hpx_gas_try_pin(target, (void**)&chan))
    return HPX_RESEND;

  uint32_t size = hpx_thread_current_args_size();
  _set_args_t sargs = { .chan = chan, .status = HPX_SUCCESS,
                        .src = (void*)args, .size = size };
  _async_set_action(&sargs);
  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static int _recv_action(int *size) {
  hpx_addr_t target = hpx_thread_current_target();
  hpx_addr_t cont = hpx_thread_current_cont();

  _chan_t *chan;
  if (!hpx_gas_try_pin(target, (void**)&chan))
    return HPX_RESEND;

  uintptr_t ptr;
  hpx_status_t status = _get(chan, *size, &ptr);
  // free ptr?
  hpx_gas_unpin(target);
  if (status == HPX_SUCCESS)
    hpx_thread_continue(*size, (void*)ptr);
  else
    hpx_thread_exit(status);
}


/// Initialize a block of futures.
static int _block_init_action(uint32_t *args) {
  hpx_addr_t target = hpx_thread_current_target();
  _chan_t *channels = NULL;

  // application level forwarding if the future block has moved
  if (!hpx_gas_try_pin(target, (void**)&channels))
    return HPX_RESEND;

  // sequentially initialize each future
  uint32_t block_size = args[0];
  for (uint32_t i = 0; i < block_size; ++i)
    _init(&channels[i]);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


/// Initialize a strided block of futures
static int _blocks_init_action(uint32_t *args) {
  hpx_addr_t base = hpx_thread_current_target();
  uint32_t block_size = args[0];
  uint32_t block_bytes = block_size * sizeof(_chan_t);
  uint32_t blocks = args[1];

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (uint32_t i = 0; i < blocks; i++) {
    hpx_addr_t block = hpx_addr_add(base, i * here->ranks * block_bytes);
    hpx_call(block, _block_init, args, 2 * sizeof(*args), and);
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);
  return HPX_SUCCESS;
}

static void HPX_CONSTRUCTOR _register_actions(void) {
  _send        = HPX_REGISTER_ACTION(_send_action);
  _recv        = HPX_REGISTER_ACTION(_recv_action);
  _async_set   = HPX_REGISTER_ACTION(_async_set_action);
  _block_init  = HPX_REGISTER_ACTION(_block_init_action);
  _blocks_init = HPX_REGISTER_ACTION(_blocks_init_action);
}

/// @}


/// ----------------------------------------------------------------------------
/// Allocate a new channel.
///
/// Channels, like other LCOs, are always allocated in the global
/// address space, because their addresses can also be used as the
/// targets of parcels.
///
/// The channel LCO does not make a distinction between its send or
/// receive endpoints. A channel LCO can either be sent or received on
/// by an HPX thread. It would normally be erroneous for a single
/// thread to try to do both at once, leading to deadlocks. When there
/// is nothing to receive on a channel, the calling thread is
/// blocked.
///
/// Channels, unlike futures, can be written to multiple times. More
/// notably, channels transfer the ownership of the buffer that is
/// sent on a channel. It is the responsibility of the receiver to
/// free a buffer that it receives on the channel.
///
/// @returns    - the global address of the allocated channel
/// ----------------------------------------------------------------------------
hpx_addr_t hpx_lco_chan_new(void) {
  hpx_addr_t chan;
  _chan_t *local = _free;
  if (local) {
    _free = (_chan_t*)local->lco.vtable;
    chan = HPX_HERE;
    char *base;
    if (!hpx_gas_try_pin(chan, (void**)&base)) {
      dbg_error("Could not translate local block.\n");
      hpx_abort();
    }
    chan.offset = (char*)local - base;
    assert(chan.offset < chan.block_bytes);
  }
  else {
    chan = hpx_gas_alloc(sizeof(_chan_t));
    if (!hpx_gas_try_pin(chan, (void**)&local)) {
      dbg_error("Could not pin newly allocated channel.\n");
      hpx_abort();
    }
  }
  _init(local);
  hpx_gas_unpin(chan);
  return chan;
}

/// ----------------------------------------------------------------------------
/// Channel send.
/// ----------------------------------------------------------------------------
void hpx_lco_chan_send(hpx_addr_t chan, const void *value, int size, hpx_addr_t sync) {
  _chan_t *c;
  if (hpx_gas_try_pin(chan, (void**)&c)) {
    _set(c, size, value, HPX_SUCCESS, sync);
    hpx_gas_unpin(chan);
  }
  else {
    // ugh, we don't have local completion semantics for parcels.
    hpx_call(chan, _send, value, size, HPX_NULL);
    if (!hpx_addr_eq(sync, HPX_NULL))
      hpx_lco_set(sync, NULL, 0, HPX_NULL);
  }
}

/// ----------------------------------------------------------------------------
/// Channel receive.
/// ----------------------------------------------------------------------------
void *hpx_lco_chan_recv(hpx_addr_t chan, int size) {
  hpx_status_t status;
  _chan_t *c;
  uintptr_t ptr;

  if (hpx_gas_try_pin(chan, (void**)&c)) {
    status = _get(c, size, &ptr);
    hpx_gas_unpin(chan);
  }
  else {
    hpx_addr_t proxy = hpx_lco_future_new(size);
    hpx_call(chan, _recv, &size, sizeof(size), proxy);
    ptr = (uintptr_t) malloc(size);
    assert(ptr != 0);
    status = hpx_lco_get(proxy, (void*)ptr, size);
    hpx_lco_delete(proxy, HPX_NULL);
  }
  if (status != HPX_SUCCESS)
    return NULL;
  else
    return (void*)ptr;
}

/// ----------------------------------------------------------------------------
/// Allocate a global array of channels.
/// ----------------------------------------------------------------------------
hpx_addr_t
hpx_lco_chan_array_new(int n, int block_size) {
  // perform the global allocation
  uint32_t blocks = (n / block_size) + ((n % block_size) ? 1 : 0);
  uint32_t block_bytes = block_size * sizeof(_chan_t);
  hpx_addr_t base = hpx_gas_global_alloc(blocks, block_bytes);

  int ranks = here->ranks;
  // for each rank, send an initialization message
  uint32_t args[2] = {
    block_size,
    (blocks / ranks) // blks per rank
  };

  int rem = blocks % ranks;
  hpx_addr_t and[2] = {
    hpx_lco_and_new(ranks),
    hpx_lco_and_new(rem)
  };

  for (int i = 0; i < ranks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes);
    hpx_call(there, _blocks_init, args, sizeof(args), and[0]);
  }

  for (int i = 0; i < rem; ++i) {
    hpx_addr_t block = hpx_addr_add(base, args[1] * ranks + i * block_bytes);
    hpx_call(block, _block_init, args, 2 * sizeof(args[0]), and[1]);
  }

  hpx_lco_wait_all(2, and);
  hpx_lco_delete(and[0], HPX_NULL);
  hpx_lco_delete(and[1], HPX_NULL);

  // return the base address of the allocation
  return base;
}


hpx_addr_t
hpx_lco_chan_array_at(hpx_addr_t array, int i) {
  return hpx_addr_add(array, i * sizeof(_chan_t));
}


void
hpx_lco_chan_array_delete(hpx_addr_t array, hpx_addr_t sync) {
  dbg_log("unimplemented");
  if (!hpx_addr_eq(sync, HPX_NULL))
    hpx_lco_set(sync, NULL, 0, HPX_NULL);
}

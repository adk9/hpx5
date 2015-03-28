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
#include "config.h"
#endif

/// @file libhpx/scheduler/chan.c
/// Defines a channel structure.

#include <assert.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/scheduler.h>
#include "cvar.h"
#include "lco.h"

/// Local channel interface.
///
/// A channel LCO maintains a linked-list of dynamically sized
/// buffers. It can be used to support a thread-based, point-to-point
/// communication mechanism. An in-order channel forces a sender to
/// wait for remote completion for sets or sends().
/// Local channel interface.
/// @{

typedef struct node {
  struct node  *next;
  void       *buffer;                           // out-of place because we want
  int           size;                           // to be able to recv it
} _chan_node_t;


typedef struct {
  lco_t          lco;
  cvar_t    nonempty;
  _chan_node_t  *head;
  _chan_node_t  *tail;
} _chan_t;

/// Internal actions.

static int _chan_enqueue(_chan_t *chan, _chan_node_t *node) {
  if (chan->tail) {
    chan->tail->next = node;
    chan->tail = node;
    return 0;
  }
  else {
    chan->head = chan->tail = node;
    return 1;
  }
}

static _chan_node_t *_chan_dequeue(_chan_t *chan) {
  _chan_node_t *node = chan->head;
  if (node == NULL) {
    return NULL;
  }

  if ((chan->head = node->next) == NULL) {
    chan->tail = NULL;
  }

  return node;
}

static size_t _chan_size(lco_t *lco) {
  _chan_t *chan = (_chan_t *)lco;
  return sizeof(*chan);
}

/// Deletes a channel and its internal buffers.
static void _chan_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  _chan_t *c = (_chan_t *)lco;
  _chan_node_t *node = NULL;
  while ((node = c->head) != NULL) {
    c->head = c->head->next;
    free(node->buffer);
    free(node);
  }
  lco_fini(lco);
  global_free(lco);
}

static void _chan_error(lco_t *lco, hpx_status_t code) {
  _chan_t *chan = (_chan_t *)lco;
  lco_lock(&chan->lco);
  scheduler_signal_error(&chan->nonempty, code);
  lco_unlock(&chan->lco);
}

/// Copies the @p from pointer into channel's buffer.
static void _chan_set(lco_t *lco, int size, const void *from) {
  // set up the node that we're going to enqueue
  _chan_node_t *node = malloc(sizeof(*node));
  node->next = NULL;
  node->size = size;
  if (size != 0) {
    dbg_assert(from);
    node->buffer = malloc(size);
    memcpy(node->buffer, from, size);
  }

  // lock the channel and enqueue the node
  lco_lock(lco);
  _chan_t *chan = (_chan_t *)lco;
  if (_chan_enqueue(chan, node)) {
    scheduler_signal(&chan->nonempty);
  }
  lco_unlock(lco);
}

/// This is a non-blocking try recv on a channel. If the buffer had no data,
/// then HPX_LCO_CHAN_EMPTY is returned, otherwise @p size is set >= 0, @p is
/// set to be the buffer, and HPX_SUCCESS is returned. If the channel has an
/// error code set, then that error code is returned.
///
/// If the return value is not HPX_SUCCESS then neither @p size nor @p buffer is
/// set.
static hpx_status_t _chan_try_recv(_chan_t *chan, int *size, void **buffer) {
  lco_lock(&chan->lco);
  hpx_status_t status = cvar_get_error(&chan->nonempty);
  if (status == HPX_SUCCESS) {
    _chan_node_t *node = _chan_dequeue(chan);
    if (!node) {
      status = HPX_LCO_CHAN_EMPTY;
    }
    else {
      *size = node->size;
      *buffer = node->buffer;
      free(node);
    }
  }
  lco_unlock(&chan->lco);
  return status;
}

/// The channel recv is like a channel get, except that we don't copy out to a
/// user supplied buffer, but instead return the buffer directly.
static hpx_status_t _chan_recv(_chan_t *chan, int *size, void **buffer) {
  lco_lock(&chan->lco);
  hpx_status_t status = cvar_get_error(&chan->nonempty);
  _chan_node_t       *node = _chan_dequeue(chan);

  while (status == HPX_SUCCESS && !node) {
    status = scheduler_wait(&chan->lco.lock, &chan->nonempty);
    node = _chan_dequeue(chan);
  }

  // don't need the lock anymore, since either there's an error or I privatized
  // the head node
  lco_unlock(&chan->lco);

  // if we got here without an error, then we dequeued succesfully
  if (status == HPX_SUCCESS) {
    if (size) {
      *size = node->size;
    }
    if (buffer) {
      *buffer = node->buffer;
    }
    free(node);
  }

  return status;
}

/// Use _chan_recv() to get the next buffer, and then copy it to the
/// user-supplied buffer.
static hpx_status_t _chan_get(lco_t *lco, int size, void *out) {
  int           bsize = 0;
  void        *buffer = NULL;
  hpx_status_t status = _chan_recv((_chan_t *)lco, &bsize, &buffer);

  if (status == HPX_SUCCESS) {
    dbg_assert(size == bsize);
    memcpy(out, buffer, bsize);
    free(buffer);
  }

  return status;
}

// For a channel, waiting simply waits until the channel is not empty, but it
// doesn't really provide any useful information.
static hpx_status_t _chan_wait(lco_t *lco) {
  lco_lock(lco);
  _chan_t        *chan = (_chan_t *)lco;
  hpx_status_t status = cvar_get_error(&chan->nonempty);
  _chan_node_t   *node = chan->head;

  while (status == HPX_SUCCESS && !node) {
    status = scheduler_wait(&chan->lco.lock, &chan->nonempty);
    node = chan->head;
  }

  lco_unlock(lco);
  return status;
}

/// Initialize the channel
static void _chan_init(_chan_t *c) {
  static const lco_class_t vtable = {
    .on_fini     = _chan_fini,
    .on_error    = _chan_error,
    .on_set      = _chan_set,
    .on_get      = _chan_get,
    .on_getref   = NULL,
    .on_release  = NULL,
    .on_wait     = _chan_wait,
    .on_attach   = NULL,
    .on_reset    = NULL,
    .on_size     = _chan_size
  };

  lco_init(&c->lco, &vtable);
  cvar_reset(&c->nonempty);
  c->head = c->tail = NULL;
}

/// Perform a receive operation on behalf of a remote recv operation. This will
/// use the parcel continuation to copy the buffer out to the actual
/// receiver. This will block in the recv if there is no buffer yet available.
static HPX_ACTION(_chan_recv_proxy, void *args) {
  hpx_addr_t target = hpx_thread_current_target();
  int size = 0;
  void *buffer = NULL;
  hpx_status_t status = hpx_lco_chan_recv(target, &size, &buffer);
  if (status != HPX_SUCCESS || size == 0) {
    hpx_thread_exit(status);
  }
  else {
    // free the buffer after we copy out its data
    dbg_assert(size > 0);
    hpx_thread_continue_cleanup(size, buffer, free, buffer);
  }
}

/// Perform a try receive operation on behalf of a remote try recv
/// operation. This will use the parcel continuation to copy the buffer out to
/// the actual receiver. This will not block, if there is no buffer available it
/// will return a custom error code.
static HPX_ACTION(_chan_try_recv_proxy, void *args) {
  int            size = 0;
  void        *buffer = NULL;
  hpx_addr_t   target = hpx_thread_current_target();
  hpx_status_t status = hpx_lco_chan_try_recv(target, &size, &buffer);
  if (status != HPX_SUCCESS || size == 0) {
    hpx_thread_exit(status);
  }
  else {
    // free the buffer after we copy out its data
    dbg_assert(size > 0);
    hpx_thread_continue_cleanup(size, buffer, free, buffer);
  }
}


/// Initialize a block of futures.
static HPX_PINNED(_block_init, _chan_t *channels, uint32_t *args) {
  // sequentially initialize each channel
  uint32_t block_size = args[0];
  for (uint32_t i = 0; i < block_size; ++i) {
    _chan_init(&channels[i]);
  }

  return HPX_SUCCESS;
}

/// @}


/// Allocate a new channel.
///
/// Channels, like other LCOs, are always allocated in the global
/// address space, because their addresses can be used as the targets
/// of parcels.
///
/// The channel LCO does not make a distinction between its send or
/// receive endpoints. A channel LCO can either be sent or received on
/// by an HPX thread. When there is nothing to receive on a channel,
/// the calling thread is blocked.
///
/// Channels, unlike futures, can be written to multiple times. More
/// notably, channels transfer the ownership of the buffer that is
/// sent on a channel. It is the responsibility of the receiver to
/// free a buffer that it receives on the channel.
///
/// @returns the global address of the allocated channel
hpx_addr_t hpx_lco_chan_new(void) {
  _chan_t *local = global_malloc(sizeof(_chan_t));
  assert(local);
  _chan_init(local);
  return lva_to_gva(local);
}

/// Channel send.
void hpx_lco_chan_send(hpx_addr_t chan, int size, const void *value,
                       hpx_addr_t lsync, hpx_addr_t rsync) {
  hpx_lco_set(chan, size, value, lsync, rsync);
}


void hpx_lco_chan_send_inorder(hpx_addr_t chan, int size, const void *value,
                               hpx_addr_t lsync) {
  hpx_addr_t rsync = hpx_lco_future_new(0);
  hpx_lco_chan_send(chan, size, value, lsync, rsync);
  hpx_lco_wait(rsync);
  hpx_lco_delete(rsync, HPX_NULL);
}


/// Channel receive.
///
/// If the channel is local, we can just do a synchronous receive to get the
/// next buffer. Otherwise, we allocate a temporary proxy channel locally, and
/// receive into it through a remote receive. We have to use a channel instead
/// of a future because we don't know the size that we're receiving, so the
/// channel needs to allocate the buffer internally on it's own.
hpx_status_t hpx_lco_chan_recv(hpx_addr_t chan, int *size, void **buffer) {
  _chan_t *c = NULL;
  hpx_status_t status = HPX_SUCCESS;
  if (hpx_gas_try_pin(chan, (void**)&c)) {
    status = _chan_recv(c, size, buffer);
    hpx_gas_unpin(chan);
  }
  else {
    hpx_addr_t proxy = hpx_lco_chan_new();
    hpx_call(chan, _chan_recv_proxy, proxy, NULL, 0);
    status = hpx_lco_chan_recv(proxy, size, buffer);
    hpx_lco_delete(proxy, HPX_NULL);
  }
  return status;
}


/// Channel try receive.
///
/// If the channel is local, we can just do a synchronous try recv, otherwise we
/// allocate a proxy channel to receive into (we need to use a channel because
/// we don't know size) and use the _chan_try_recv action to get the remote
/// value.
hpx_status_t hpx_lco_chan_try_recv(hpx_addr_t chan, int *size, void **buffer) {
  _chan_t          *c = NULL;
  hpx_status_t status = HPX_SUCCESS;
  if (hpx_gas_try_pin(chan, (void**)&c)) {
    status = _chan_try_recv(c, size, buffer);
    hpx_gas_unpin(chan);
  }
  else {
    hpx_addr_t proxy = hpx_lco_chan_new();
    hpx_call(chan, _chan_try_recv_proxy, proxy, NULL, 0);
    status = hpx_lco_chan_try_recv(proxy, size, buffer);
    hpx_lco_delete(proxy, HPX_NULL);
  }
  return status;
}


/// Allocate a global array of channels.
///
/// @param          n the total number of channels to allocate
/// @param block_size the number of channels per block
hpx_addr_t hpx_lco_chan_array_new(int n, int size, int chans_per_block) {
  // perform the global allocation
  uint32_t     blocks   = ceil_div_32(n, chans_per_block);;
  uint32_t chan_bytes   = sizeof(_chan_t) + size;
  uint32_t  block_bytes = chans_per_block * chan_bytes;
  hpx_addr_t       base = hpx_gas_alloc_cyclic(blocks, block_bytes, 0);

  // for each rank, send an initialization message
  uint32_t args[] = { chans_per_block, size };

  hpx_addr_t and = hpx_lco_and_new(blocks);
  for (int i = 0; i < blocks; ++i) {
    hpx_addr_t there = hpx_addr_add(base, i * block_bytes, block_bytes);
    int e = hpx_call(there, _block_init, and, args, sizeof(args));
    dbg_check(e, "call of _block_init failed\n");
  }
  hpx_lco_wait(and);
  hpx_lco_delete(and, HPX_NULL);

  // return the base address of the allocation
  return base;
}


hpx_addr_t hpx_lco_chan_array_at(hpx_addr_t array, int i, int size, int bsize) {
  uint32_t chan_bytes = sizeof(_chan_t) + size;
  uint32_t  block_bytes = bsize * chan_bytes;
  return hpx_addr_add(array, i * (sizeof(_chan_t) + size), block_bytes);
}


void hpx_lco_chan_array_delete(hpx_addr_t array, hpx_addr_t sync) {
  log_lco("chan: array delete unimplemented");
  if (sync) {
    hpx_lco_set(sync, 0, NULL, HPX_NULL, HPX_NULL);
  }
}


hpx_status_t hpx_lco_chan_array_select(int n, hpx_addr_t channels[], int *i,
                                       int *size, void **out) {
  *i = 0;
  hpx_status_t status = hpx_lco_chan_try_recv(channels[*i], size, out);
  while (status == HPX_LCO_CHAN_EMPTY) {
    *i = (*i + 1) % n;
    status = hpx_lco_chan_try_recv(channels[*i], size, out);
  }
  return status;
}

/// Initialize a block of array of lco.
static HPX_PINNED(_block_local_init, void *lco, uint32_t *args) {
  for (int i = 0; i < args[0]; i++) {
    void *addr = (void *)((uintptr_t)lco + i * (sizeof(_chan_t) + args[1]));
    _chan_init(addr);
  }
  return HPX_SUCCESS;
}

/// Allocate an array of chan local to the calling locality.
/// @param          n The (total) number of futures to allocate
/// @param       size The size of each future's value 
///
/// @returns the global address of the allocated array lco.
hpx_addr_t hpx_lco_chan_local_array_new(int n, int size) {
  uint32_t lco_bytes = sizeof(_chan_t) + size;
  dbg_assert(n * lco_bytes < UINT32_MAX);
  uint32_t  block_bytes = n * lco_bytes;
  hpx_addr_t base = hpx_gas_alloc_local(block_bytes, 0);

  // for each block, initialize the future.
  uint32_t args[] = {n, size};
  int e = hpx_call_sync(base, _block_local_init, NULL, 0, &args, sizeof(args));
  dbg_check(e, "call of _block_init_action failed\n");

  return base;
}
 
